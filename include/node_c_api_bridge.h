#pragma once

#include "animation_frame_queue.h"
#include "timer_queue.h"

// A C++-only companion to node_c_api.h, for the one thing there that
// can't be plain C: handing node_c_api.cpp the TimerQueue/
// AnimationFrameQueue references it needs to back
// ArtisanSetTimeout/SetInterval/RequestAnimationFrame - node_c_api.h
// itself is parsed directly by cgo (see go/artisango/node.go's `#include
// "node_c_api.h"`), so it has to stay free of C++ types. Included by
// main.cpp (the caller) and node_c_api.cpp (the implementation), never
// by anything cgo touches.

namespace artisan {

// Called once per navigate() (main.cpp), before ArtisanSetupApp, so a
// Go app's SetupApp can schedule into `timers`/`animationFrames`
// immediately - same references JsEngine's constructor already takes.
void SetGoTimerContext(TimerQueue &timers, AnimationFrameQueue &animationFrames);

} // namespace artisan
