#include "link_state.h"

#include "config.h"

#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t kQueueSize = 8;

LinkRequest g_requests[kQueueSize]{};
std::size_t g_request_head  = 0;
std::size_t g_request_count = 0;
bool g_ready                = false;
bool g_window_open          = false;
LinkPairingPhase g_phase    = LinkPairingPhase::Closed;
uint32_t g_window_deadline  = 0;
uint32_t g_now              = 0;
uint32_t g_resume_gen       = 0;

void push_request(LinkRequest request) {
    if (g_request_count >= kQueueSize) {
        return;
    }
    const std::size_t index = (g_request_head + g_request_count) % kQueueSize;
    g_requests[index]       = request;
    g_request_count += 1U;
}

void open_window() {
    g_window_open     = true;
    g_phase           = LinkPairingPhase::Advertising;
    g_window_deadline = g_now + PAIRING_WINDOW_MS;
}

// Bond success and the window deadline. NoRadio and END_PAIRING do not.
void note_resume() {
    g_resume_gen += 1U;
}

void close_window(LinkPairingPhase phase) {
    g_window_open = false;
    g_phase       = phase;
}

} // namespace

// Deadline is measured from link_tick's timestamp. loop_dispatch runs after
// link_tick on the same pass, so BEGIN_PAIRING opens at that millisecond.
void link_tick(uint32_t now) {
    g_now = now;
    if (!g_window_open || now < g_window_deadline) {
        return;
    }
    close_window(LinkPairingPhase::Closed);
    note_resume();
    push_request(LinkRequest{LinkRequestKind::EndPairing});
}

void link_on_request(LinkRequest request) {
    switch (request.kind) {
    case LinkRequestKind::BeginPairing:
        // A second chord restarts the deadline and advertises again (§7.5).
        open_window();
        push_request(request);
        break;
    case LinkRequestKind::EndPairing:
        if (g_window_open) {
            close_window(LinkPairingPhase::Closed);
        }
        push_request(request);
        break;
    }
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
        if (g_window_open) {
            g_phase = LinkPairingPhase::Bonding;
        }
        break;
    case LinkEventKind::PairingFailed:
        // Deadline is not extended (§7.5).
        if (g_window_open) {
            g_phase = LinkPairingPhase::Advertising;
        }
        break;
    case LinkEventKind::PairingSucceeded:
        if (g_window_open) {
            close_window(LinkPairingPhase::BondOk);
            note_resume();
        }
        break;
    case LinkEventKind::NoRadio:
        // Closes immediately. No END_PAIRING, and no inactivity restart (Q035).
        if (g_window_open) {
            close_window(LinkPairingPhase::Closed);
        }
        break;
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
    return g_window_open;
}

LinkPairingPhase link_pairing_phase() {
    return g_phase;
}

uint32_t link_pairing_resume_generation() {
    return g_resume_gen;
}

bool link_ready() {
    return g_ready;
}

bool link_boot_connected() {
    return false;
}

#ifdef HOST_TEST
void link_reset() {
    g_request_head    = 0;
    g_request_count   = 0;
    g_ready           = false;
    g_window_open     = false;
    g_phase           = LinkPairingPhase::Closed;
    g_window_deadline = 0;
    g_now             = 0;
    g_resume_gen      = 0;
}
#endif
