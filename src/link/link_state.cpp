#include "link_state.h"

#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t kQueueSize = 8;

LinkRequest g_requests[kQueueSize]{};
std::size_t g_request_head  = 0;
std::size_t g_request_count = 0;
bool g_ready                = false;

void push_request(LinkRequest request) {
    if (g_request_count >= kQueueSize) {
        return;
    }
    const std::size_t index = (g_request_head + g_request_count) % kQueueSize;
    g_requests[index]       = request;
    g_request_count += 1U;
}

} // namespace

// T32 skeleton. The window never opens, so there is no deadline to expire.
// T36 owns the window; T37 owns the boot-connect latch.
void link_tick(uint32_t now) {
    static_cast<void>(now);
}

// T32: forwarded to the transport unchanged. T36 puts the window in front.
void link_on_request(LinkRequest request) {
    push_request(request);
}

void link_on_event(LinkEvent event) {
    switch (event.kind) {
    case LinkEventKind::Ready:
        g_ready = true;
        break;
    case LinkEventKind::Lost:
        g_ready = false;
        break;
    case LinkEventKind::PairingStarted:
    case LinkEventKind::PairingSucceeded:
    case LinkEventKind::PairingFailed:
    case LinkEventKind::NoRadio:
        break; // T36
    }
}

bool link_poll_request(LinkRequest* out) {
    if (out == nullptr || g_request_count == 0U) {
        return false;
    }
    *out           = g_requests[g_request_head];
    g_request_head = (g_request_head + 1U) % kQueueSize;
    g_request_count -= 1U;
    return true;
}

bool link_pairing_window_open() {
    return false;
}

LinkPairingPhase link_pairing_phase() {
    return LinkPairingPhase::Closed;
}

bool link_ready() {
    return g_ready;
}

bool link_boot_connected() {
    return false;
}

#ifdef HOST_TEST
void link_reset() {
    g_request_head  = 0;
    g_request_count = 0;
    g_ready         = false;
}
#endif
