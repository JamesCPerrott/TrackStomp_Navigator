#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// BLE peripheral only. Classic, LE central, GATT client, stdin, and RTT
// stay off. NVM_NUM_DEVICE_DB_ENTRIES is the bond cap; ble_midi.cpp
// static_asserts it equals BLE_MAX_BONDS (config.h is C++).

#ifdef DEBUG_UART
#define ENABLE_LOG_INFO
#define ENABLE_LOG_ERROR
#endif

// The SDK always compiles hci_dump_embedded_stdout.c, which refuses to build
// without this. The dump is started only when DEBUG_UART sets WANT_HCI_DUMP,
// and the linker drops the unused functions from a release binary.
#define ENABLE_PRINTF_HEXDUMP

#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION
#define ENABLE_LE_SECURE_CONNECTIONS
#define ENABLE_LE_ENHANCED_CONNECTION_COMPLETE_EVENT
#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS
#define ENABLE_SOFTWARE_AES128

// CYW43 shared-bus limits from the SDK's Pico W BTstack examples.
#define ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
#define MAX_NR_CONTROLLER_ACL_BUFFERS 3
#define MAX_NR_CONTROLLER_SCO_PACKETS 3
#define HCI_HOST_ACL_PACKET_LEN       1024
#define HCI_HOST_ACL_PACKET_NUM       3
#define HCI_HOST_SCO_PACKET_LEN       120
#define HCI_HOST_SCO_PACKET_NUM       3

#define HCI_OUTGOING_PRE_BUFFER_SIZE 4
#define HCI_ACL_PAYLOAD_SIZE         (1691 + 4)
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT 4

#define MAX_NR_HCI_CONNECTIONS   1
#define MAX_NR_L2CAP_CHANNELS    3
#define MAX_NR_L2CAP_SERVICES    2
#define MAX_NR_SM_LOOKUP_ENTRIES 2
#define MAX_NR_WHITELIST_ENTRIES 1
#define MAX_NR_GATT_CLIENTS      1

#define NVM_NUM_DEVICE_DB_ENTRIES 1

#define MAX_ATT_DB_SIZE 512

#define HAVE_EMBEDDED_TIME_MS
#define HAVE_ASSERT
#define HCI_RESET_RESEND_TIMEOUT_MS 1000

#endif
