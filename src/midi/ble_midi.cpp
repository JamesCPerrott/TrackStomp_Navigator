#include "ble_midi.h"

#ifndef HOST_TEST

#include "ble_midi_packet.h"
#include "btstack.h"
#include "config.h"
#include "trackstomp.h"

#ifdef DEBUG_UART
#include "pico.h"

#include <cinttypes>
#include <cstdio>
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>

// NVM_NUM_DEVICE_DB_ENTRIES lives in btstack_config.h, which cannot include
// this C++ header. The two must stay the same number.
static_assert(NVM_NUM_DEVICE_DB_ENTRIES == BLE_MAX_BONDS);

namespace {

constexpr std::size_t kEventQueue = 8;
constexpr std::size_t kSendQueue  = 4;

// 1 advertising-interval unit = 0.625 ms. 100 ms -> 160.
constexpr uint16_t kAdvIntervalUnits = static_cast<uint16_t>((BLE_ADV_INTERVAL_MS * 8U) / 5U);
static_assert(kAdvIntervalUnits == 160U);
static_assert(kAdvIntervalUnits >= 0x0020U);
static_assert(kAdvIntervalUnits <= 0x4000U);

// Connectable undirected, all three channels, no accept list (Q030).
constexpr uint8_t kAdvTypeInd  = 0;
constexpr uint8_t kAdvChannels = 0x07;
constexpr uint8_t kAdvFilter   = 0;
constexpr uint8_t kAdvFlags    = 0x06; // general discoverable, BR/EDR not supported

// Q037. The PRD names the connection interval only.
constexpr uint16_t kConnLatency             = 0;
constexpr uint16_t kSupervisionTimeoutUnits = 500;

// BLE-MIDI service 03B80E5A-EDE8-4B33-A751-6CE34EC4C700, little-endian.
constexpr uint8_t kAdvData[] = {
    0x02,
    BLUETOOTH_DATA_TYPE_FLAGS,
    kAdvFlags,
    0x11,
    BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS,
    0x00,
    0xC7,
    0xC4,
    0x4E,
    0xE3,
    0x6C,
    0x51,
    0xA7,
    0x33,
    0x4B,
    0xE8,
    0xED,
    0x5A,
    0x0E,
    0xB8,
    0x03,
};
static_assert(sizeof(kAdvData) == 21U);
static_assert(sizeof(kAdvData) <= 31U);

constexpr char kLocalName[] = "TrackStomp Navigator";
static_assert(sizeof(kLocalName) == 21U);

constexpr uint16_t kMidiValue =
    ATT_CHARACTERISTIC_7772E5DB_3868_4112_A1A9_F2669D106BF3_01_VALUE_HANDLE;
constexpr uint16_t kMidiCccd =
    ATT_CHARACTERISTIC_7772E5DB_3868_4112_A1A9_F2669D106BF3_01_CLIENT_CONFIGURATION_HANDLE;

LinkEvent g_events[kEventQueue]{};
std::size_t g_event_head  = 0;
std::size_t g_event_count = 0;

struct PendingSend {
    uint8_t bytes[BLE_MIDI_PACKET_LENGTH];
};

PendingSend g_sends[kSendQueue]{};
std::size_t g_send_head  = 0;
std::size_t g_send_count = 0;

hci_con_handle_t g_handle = HCI_CON_HANDLE_INVALID;
btstack_timer_source_t g_drop_timer{};
btstack_packet_callback_registration_t g_hci_registration{};
btstack_packet_callback_registration_t g_sm_registration{};

uint8_t g_adv[sizeof(kAdvData)]{};
uint8_t g_scan[2U + sizeof(kLocalName) - 1U]{};

bool g_started      = false;
bool g_hci_working  = false;
bool g_no_radio     = false;
bool g_adv_on       = false;
bool g_bondable     = false;
bool g_adv_filled   = false;
bool g_encrypted    = false;
bool g_subscribed   = false;
bool g_link_ready   = false;
bool g_send_waiting = false;
bool g_drop_armed   = false;

#ifdef DEBUG_UART
volatile uint32_t g_bond_erases   = 0;
volatile uint32_t g_bond_programs = 0;
uint32_t g_reported_erases        = 0;
uint32_t g_reported_programs      = 0;
#endif

void push_event(LinkEventKind kind) {
    if (g_event_count >= kEventQueue) {
        return;
    }
    const std::size_t index = (g_event_head + g_event_count) % kEventQueue;
    g_events[index]         = LinkEvent{kind};
    g_event_count += 1U;
}

bool pop_event(LinkEvent* out) {
    if (out == nullptr || g_event_count == 0U) {
        return false;
    }
    *out         = g_events[g_event_head];
    g_event_head = (g_event_head + 1U) % kEventQueue;
    g_event_count -= 1U;
    return true;
}

void clear_sends() {
    g_send_head    = 0;
    g_send_count   = 0;
    g_send_waiting = false;
}

void request_send();

void publish_ready() {
    if (!g_encrypted || !g_subscribed || g_link_ready) {
        return;
    }
    g_link_ready = true;
    push_event(LinkEventKind::Ready);
    request_send();
}

void publish_lost() {
    if (!g_link_ready) {
        return;
    }
    g_link_ready = false;
    clear_sends();
    push_event(LinkEventKind::Lost);
}

void request_send() {
    if (g_send_waiting || g_send_count == 0U || !g_link_ready) {
        return;
    }
    if (g_handle == HCI_CON_HANDLE_INVALID) {
        return;
    }
    // BTstack delivers CAN_SEND_NOW from inside this call when a buffer is
    // free. The flag has to be set first: on_can_send clears it, and setting
    // it afterwards leaves every later note queued until disconnect.
    g_send_waiting = true;
    att_server_request_can_send_now_event(g_handle);
}

void fill_ad() {
    if (g_adv_filled) {
        return;
    }
    std::memcpy(g_adv, kAdvData, sizeof(kAdvData));
    g_scan[0] = static_cast<uint8_t>(sizeof(g_scan) - 1U);
    g_scan[1] = BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME;
    std::memcpy(&g_scan[2], kLocalName, sizeof(kLocalName) - 1U);
    g_adv_filled = true;
}

void apply_advertising() {
    if (!g_hci_working) {
        return;
    }
    fill_ad();
    const bool window    = link_pairing_window_open();
    const bool connected = g_handle != HCI_CON_HANDLE_INVALID;
    const bool bonded    = le_device_db_count() > 0;
    const bool enable    = !connected && (window || bonded);
    if (enable != g_adv_on) {
        if (enable) {
            bd_addr_t direct{};
            gap_advertisements_set_params(kAdvIntervalUnits, kAdvIntervalUnits, kAdvTypeInd, 0,
                                          direct, kAdvChannels, kAdvFilter);
            gap_advertisements_set_data(static_cast<uint8_t>(sizeof(g_adv)), g_adv);
            gap_scan_response_set_data(static_cast<uint8_t>(sizeof(g_scan)), g_scan);
        }
        gap_advertisements_enable(enable ? 1 : 0);
        g_adv_on = enable;
    }
    if (window != g_bondable) {
        gap_set_bondable_mode(window ? 1 : 0);
        g_bondable = window;
    }
}

void cancel_drop_timer() {
    if (!g_drop_armed) {
        return;
    }
    btstack_run_loop_remove_timer(&g_drop_timer);
    g_drop_armed = false;
}

void arm_drop_timer();

void mark_encrypted() {
    g_encrypted = true;
    cancel_drop_timer();
    publish_ready();
}

void on_connected(hci_con_handle_t handle) {
    if (g_handle != HCI_CON_HANDLE_INVALID) {
        gap_disconnect(handle);
        return;
    }
    g_handle     = handle;
    g_encrypted  = false;
    g_subscribed = false;
    arm_drop_timer();
    static_cast<void>(gap_request_connection_parameter_update(
        handle, BLE_CONN_INTERVAL_MIN_UNITS, BLE_CONN_INTERVAL_MAX_UNITS, kConnLatency,
        kSupervisionTimeoutUnits));
    apply_advertising();
}

void on_disconnected(hci_con_handle_t handle) {
    if (g_handle != HCI_CON_HANDLE_INVALID && handle != g_handle) {
        return;
    }
    cancel_drop_timer();
    g_handle     = HCI_CON_HANDLE_INVALID;
    g_encrypted  = false;
    g_subscribed = false;
    publish_lost();
    apply_advertising();
}

// gap_delete_bonding removes the TLV entry and reloads the resolving list.
// A second pass drops anything the address lookup missed.
void clear_bonds() {
    const int max_count = le_device_db_max_count();
    for (int index = 0; index < max_count; ++index) {
        int addr_type = static_cast<int>(BD_ADDR_TYPE_UNKNOWN);
        bd_addr_t addr{};
        le_device_db_info(index, &addr_type, addr, nullptr);
        if (addr_type == static_cast<int>(BD_ADDR_TYPE_UNKNOWN)) {
            continue;
        }
        gap_delete_bonding(static_cast<bd_addr_type_t>(addr_type), addr);
    }
    for (int index = 0; index < max_count; ++index) {
        int addr_type = static_cast<int>(BD_ADDR_TYPE_UNKNOWN);
        bd_addr_t addr{};
        le_device_db_info(index, &addr_type, addr, nullptr);
        if (addr_type != static_cast<int>(BD_ADDR_TYPE_UNKNOWN)) {
            le_device_db_remove(index);
        }
    }
}

void on_can_send() {
    g_send_waiting = false;
    if (!g_link_ready || g_handle == HCI_CON_HANDLE_INVALID || g_send_count == 0U) {
        clear_sends();
        return;
    }
    const PendingSend pending = g_sends[g_send_head];
    g_send_head               = (g_send_head + 1U) % kSendQueue;
    g_send_count -= 1U;
    static_cast<void>(
        att_server_notify(g_handle, kMidiValue, pending.bytes, BLE_MIDI_PACKET_LENGTH));
    request_send();
}

#ifdef DEBUG_UART
void report_bond_bank() {
    const uint32_t erases   = g_bond_erases;
    const uint32_t programs = g_bond_programs;
    if (erases == g_reported_erases && programs == g_reported_programs) {
        return;
    }
    g_reported_erases   = erases;
    g_reported_programs = programs;
    printf("bond bank erases %" PRIu32 " programs %" PRIu32 "\n", erases, programs);
}
#endif

} // namespace

extern "C" {

static void on_unbonded_drop(btstack_timer_source_t* timer) {
    static_cast<void>(timer);
    g_drop_armed = false;
    if (g_handle != HCI_CON_HANDLE_INVALID && !g_encrypted) {
        gap_disconnect(g_handle);
    }
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    static_cast<void>(channel);
    static_cast<void>(size);
    if (packet_type != HCI_EVENT_PACKET || packet == nullptr) {
        return;
    }
    switch (hci_event_packet_get_type(packet)) {
    case BTSTACK_EVENT_STATE:
        if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
            g_hci_working = true;
            apply_advertising();
        }
        break;
    case HCI_EVENT_META_GAP:
        if (hci_event_gap_meta_get_subevent_code(packet) == GAP_SUBEVENT_LE_CONNECTION_COMPLETE &&
            gap_subevent_le_connection_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
            on_connected(gap_subevent_le_connection_complete_get_connection_handle(packet));
        }
        break;
    case HCI_EVENT_DISCONNECTION_COMPLETE:
        on_disconnected(hci_event_disconnection_complete_get_connection_handle(packet));
        break;
    case ATT_EVENT_CAN_SEND_NOW:
        on_can_send();
        break;
    case SM_EVENT_JUST_WORKS_REQUEST:
        if (link_pairing_window_open()) {
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
        } else {
            sm_bonding_decline(sm_event_just_works_request_get_handle(packet));
        }
        break;
    case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
        sm_bonding_decline(sm_event_numeric_comparison_request_get_handle(packet));
        break;
    case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
        sm_bonding_decline(sm_event_passkey_display_number_get_handle(packet));
        break;
    case SM_EVENT_PASSKEY_INPUT_NUMBER:
        sm_bonding_decline(sm_event_passkey_input_number_get_handle(packet));
        break;
    case SM_EVENT_AUTHORIZATION_REQUEST:
        sm_authorization_decline(sm_event_authorization_request_get_handle(packet));
        break;
    case SM_EVENT_PAIRING_STARTED:
        if (link_pairing_window_open()) {
            push_event(LinkEventKind::PairingStarted);
        }
        break;
    case SM_EVENT_PAIRING_COMPLETE:
        if (sm_event_pairing_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
            mark_encrypted();
            if (link_pairing_window_open()) {
                push_event(LinkEventKind::PairingSucceeded);
            }
        } else if (link_pairing_window_open()) {
            push_event(LinkEventKind::PairingFailed);
        }
        break;
    case SM_EVENT_REENCRYPTION_COMPLETE:
        if (sm_event_reencryption_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
            mark_encrypted();
        }
        break;
    default:
        break;
    }
}

static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle,
                                  uint16_t offset, uint8_t* buffer, uint16_t buffer_size) {
    static_cast<void>(connection_handle);
    if (att_handle != kMidiValue) {
        return 0;
    }
    // att_read_callback_handle_blob asserts a non-null blob. Size 0 is empty.
    static const uint8_t empty = 0;
    return att_read_callback_handle_blob(&empty, 0, offset, buffer, buffer_size);
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle,
                              uint16_t transaction_mode, uint16_t offset, uint8_t* buffer,
                              uint16_t buffer_size) {
    static_cast<void>(connection_handle);
    static_cast<void>(transaction_mode);
    static_cast<void>(offset);
    if (att_handle == kMidiCccd) {
        uint16_t value = 0;
        if (buffer != nullptr && buffer_size >= 2U) {
            value = little_endian_read_16(buffer, 0);
        }
        const bool notify = (value & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION) != 0U;
        g_subscribed      = notify;
        if (notify) {
            publish_ready();
        } else {
            publish_lost();
        }
    }
    return 0;
}

#ifdef DEBUG_UART
extern "C" void __real_flash_range_erase(uint32_t flash_offs, size_t count);
extern "C" void __real_flash_range_program(uint32_t flash_offs, const uint8_t* data, size_t count);

// Called with XIP off. The comparison stays in this function so it is not
// a call into flash. The UART print happens later, from poll.
void __no_inline_not_in_flash_func(__wrap_flash_range_erase)(uint32_t flash_offs, size_t count) {
    if (flash_offs >= BOND_FLASH_OFFSET && flash_offs < (BOND_FLASH_OFFSET + BOND_FLASH_SIZE)) {
        g_bond_erases = g_bond_erases + 1U;
    }
    __real_flash_range_erase(flash_offs, count);
}

void __no_inline_not_in_flash_func(__wrap_flash_range_program)(uint32_t flash_offs,
                                                               const uint8_t* data, size_t count) {
    if (flash_offs >= BOND_FLASH_OFFSET && flash_offs < (BOND_FLASH_OFFSET + BOND_FLASH_SIZE)) {
        g_bond_programs = g_bond_programs + 1U;
    }
    __real_flash_range_program(flash_offs, data, count);
}
#endif

} // extern "C"

namespace {

void arm_drop_timer() {
    if (g_drop_armed) {
        btstack_run_loop_remove_timer(&g_drop_timer);
    }
    btstack_run_loop_set_timer_handler(&g_drop_timer, &on_unbonded_drop);
    btstack_run_loop_set_timer(&g_drop_timer, BLE_UNBONDED_DROP_MS);
    btstack_run_loop_add_timer(&g_drop_timer);
    g_drop_armed = true;
}

} // namespace

void ble_midi_init() {
    l2cap_init();
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(
        static_cast<uint8_t>(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING));
    sm_set_secure_connections_only_mode(true);
    gap_set_bondable_mode(0);

    att_server_init(profile_data, &att_read_callback, &att_write_callback);

    g_hci_registration.callback = &packet_handler;
    hci_add_event_handler(&g_hci_registration);
    g_sm_registration.callback = &packet_handler;
    sm_add_event_handler(&g_sm_registration);
    att_server_register_packet_handler(&packet_handler);

    g_started = true;
    hci_power_control(HCI_POWER_ON);
}

void ble_midi_request(LinkRequest request) {
    if (!g_started) {
        if (request.kind == LinkRequestKind::BeginPairing) {
            g_no_radio = true;
        }
        return;
    }
    if (request.kind == LinkRequestKind::BeginPairing) {
        clear_bonds();
        if (g_handle != HCI_CON_HANDLE_INVALID) {
            gap_disconnect(g_handle);
        }
    }
    apply_advertising();
}

bool ble_midi_poll_event(LinkEvent* out) {
#ifdef DEBUG_UART
    report_bond_bank();
#endif
    if (g_started) {
        apply_advertising();
    }
    if (out == nullptr) {
        return false;
    }
    if (g_no_radio) {
        g_no_radio = false;
        *out       = LinkEvent{LinkEventKind::NoRadio};
        return true;
    }
    return pop_event(out);
}

void ble_midi_send(const uint8_t* packet, uint8_t len) {
    if (!g_started || !g_link_ready || packet == nullptr || len != BLE_MIDI_PACKET_LENGTH) {
        return;
    }
    if (g_send_count >= kSendQueue) {
        return;
    }
    PendingSend pending{};
    std::memcpy(pending.bytes, packet, BLE_MIDI_PACKET_LENGTH);
    const std::size_t index = (g_send_head + g_send_count) % kSendQueue;
    g_sends[index]          = pending;
    g_send_count += 1U;
    request_send();
}

#endif
