#include "animation_frame_queue.h"

#include <utility>

namespace artisan {

int AnimationFrameQueue::Schedule(Callback callback) {
  int id = nextId_++;
  pending_.push_back(Pending{id, std::move(callback), /*cancelled=*/false});
  return id;
}

void AnimationFrameQueue::Cancel(int id) {
  for (Pending &p : pending_) {
    if (p.id == id) {
      p.cancelled = true;
      return;
    }
  }
}

bool AnimationFrameQueue::FireAll(double timestampMs) {
  if (pending_.empty()) {
    return false;
  }

  // Moved out (and pending_ left empty) before invoking anything - see
  // the class comment for why that's what makes a callback scheduling a
  // new one mid-call safe (it lands in the now-empty pending_, not this
  // local batch).
  std::vector<Pending> batch = std::move(pending_);
  pending_.clear();

  bool firedAny = false;
  for (const Pending &p : batch) {
    if (!p.cancelled) {
      firedAny = true;
      p.callback(timestampMs);
    }
  }
  return firedAny;
}

} // namespace artisan
