#include "timer_queue.h"

#include <algorithm>

namespace artisan {

int TimerQueue::Schedule(Callback callback, uint32_t nowMs, uint32_t delayMs,
                          bool repeating) {
  int id = nextId_++;
  timers_.push_back(Timer{id, nowMs + delayMs, delayMs, repeating,
                           /*cancelled=*/false, std::move(callback)});
  return id;
}

void TimerQueue::Cancel(int id) {
  auto it = std::find_if(timers_.begin(), timers_.end(),
                          [id](const Timer &t) { return t.id == id; });
  if (it != timers_.end()) {
    it->cancelled = true;
  }
}

bool TimerQueue::FireDue(uint32_t nowMs) {
  // A callback firing here can itself Schedule/Cancel (mutating
  // timers_ - a push_back can reallocate, invalidating any iterator or
  // pointer taken before it) - the "snapshot due ids before running any
  // callback, then re-find by id each time, copy the callback out before
  // invoking it" shape here means a timer a callback schedules mid-batch
  // can never fire within this same FireDue call (it's simply not in
  // dueIds, computed before that Schedule call happened), even if its
  // delay is 0 - it'll be picked up next FireDue call instead, avoiding
  // an unbounded same-call chain, and matching real setTimeout(fn, 0)
  // never firing synchronously either.
  std::vector<int> dueIds;
  for (const Timer &t : timers_) {
    if (!t.cancelled && t.fireAtMs <= nowMs) {
      dueIds.push_back(t.id);
    }
  }

  bool firedAny = false;

  for (int id : dueIds) {
    auto it = std::find_if(timers_.begin(), timers_.end(),
                            [id](const Timer &t) { return t.id == id; });
    if (it == timers_.end() || it->cancelled) {
      continue; // Cancelled by an earlier callback in this same batch.
    }

    Callback callback = it->callback;
    bool repeating = it->repeating;
    uint32_t delayMs = it->delayMs;

    if (repeating) {
      // Relative to *now* (when it actually fired), not it->fireAtMs
      // (when it was due) - see Schedule's doc comment for why.
      it->fireAtMs = nowMs + delayMs;
    } else {
      timers_.erase(it);
    }

    firedAny = true;
    callback();
  }

  return firedAny;
}

} // namespace artisan
