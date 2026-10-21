#pragma once

// Internal helpers for posting the public API events (include/beetrootskin/api). The events are
// registered with LeviLamina's EventBus by src/api/Api.cpp during BeetrootSkin::load().

#include "ll/api/event/EventBus.h"

#include <string_view>

namespace beetroot::api_impl {

/// Log (once per change, never per frame) that listeners started / stopped cancelling the event
/// named `name`. Client thread.
void traceCancelled(std::string_view name, bool cancelled);

/// Publish a non-cancellable event (no-op without listeners).
template <class E>
void post(E& ev) {
    ll::event::EventBus::getInstance().publish(ev);
}

/// Publish a cancellable event; true when it may proceed (not cancelled). Cancellation changes
/// are logged (traceCancelled) for e2e assertions.
template <class E>
[[nodiscard]] bool postCancellable(E& ev) {
    ll::event::EventBus::getInstance().publish(ev);
    bool cancelled = ev.isCancelled();
    traceCancelled(E::CustomEventId.name, cancelled);
    return !cancelled;
}

} // namespace beetroot::api_impl
