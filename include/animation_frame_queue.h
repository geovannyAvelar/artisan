#pragma once

#include <functional>
#include <vector>

namespace artisan {

// requestAnimationFrame/cancelAnimationFrame's backing store - separate
// from TimerQueue (timer_queue.h) despite the similar shape, because the
// callback signature differs (a timestamp argument, no delay) and so
// does the firing rule: TimerQueue fires whatever's *due*; this fires
// *everything pending*, once, every time FireAll runs - matching real
// requestAnimationFrame's "runs once before the next repaint," not "runs
// after some elapsed time." A callback that itself calls
// requestAnimationFrame (the normal way to drive a continuous animation)
// schedules for the *next* FireAll call, never the current one - see
// FireAll's own comment for why that's safe by construction, not just a
// documented rule.
class AnimationFrameQueue {
public:
  using Callback = std::function<void(double timestampMs)>;

  // Returns an id Cancel() can later use.
  int Schedule(Callback callback);

  // No-op if `id` doesn't name a still-pending (not yet fired, not
  // already cancelled) callback.
  void Cancel(int id);

  // Runs every still-pending, non-cancelled callback exactly once, each
  // with `timestampMs` (typically SDL_GetTicks(), same clock TimerQueue's
  // caller already passes it), then clears the pending list - a callback
  // that schedules a new one mid-call pushes into what is by then an
  // already-emptied list, so it can never run within this same FireAll
  // call, only a future one. Returns whether anything actually ran, so a
  // caller knows whether a redraw is warranted.
  bool FireAll(double timestampMs);

private:
  struct Pending {
    int id;
    Callback callback;
    bool cancelled;
  };

  std::vector<Pending> pending_;
  int nextId_ = 1;
};

} // namespace artisan
