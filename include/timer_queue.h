#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace artisan {

// setTimeout/setInterval's backing store - a runtime-loop concern, not a
// DOM one (nothing here touches Node), so this lives independently and
// is owned by whoever runs the event loop (main.cpp), the same footing
// as its other loop-local state (InputFocus, scroll position, ...) - not
// by JsEngine, which just holds a reference to schedule/cancel into it.
class TimerQueue {
public:
  using Callback = std::function<void()>;

  // `nowMs` (typically SDL_GetTicks(), same clock FireDue's caller
  // passes it) plus `delayMs` is when this fires - explicit rather than
  // TimerQueue reading a clock itself, so this class stays engine-
  // agnostic (no SDL dependency) and deterministic to test. Returns an id
  // Cancel() can later use. `repeating`: fires roughly every `delayMs`,
  // until cancelled - a one-shot timer removes itself after firing; a
  // repeating one reschedules for `delayMs` after the moment it actually
  // fired (not the moment it was originally due), so a FireDue call
  // that's running late can't trigger a catch-up burst of a fast
  // repeating timer all at once.
  int Schedule(Callback callback, uint32_t nowMs, uint32_t delayMs,
               bool repeating);

  // No-op if `id` doesn't name a still-pending timer (already fired
  // (one-shot) or already cancelled).
  void Cancel(int id);

  // Runs every callback whose fire time is <= `nowMs` (typically
  // SDL_GetTicks()), removing one-shot timers and rescheduling repeating
  // ones. Returns whether anything fired, so a caller knows whether a
  // redraw is warranted. A callback that schedules or cancels a timer
  // from within FireDue is safe (see the .cpp for why).
  bool FireDue(uint32_t nowMs);

private:
  struct Timer {
    int id;
    uint32_t fireAtMs;
    uint32_t delayMs;
    bool repeating;
    bool cancelled;
    Callback callback;
  };

  std::vector<Timer> timers_;
  int nextId_ = 1;
};

} // namespace artisan
