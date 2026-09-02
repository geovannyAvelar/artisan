#pragma once

// A C++-only companion to art_bridge.h, for the one thing there that
// can't be plain C: handing art_bridge.cpp the live Node& an ART
// handler's ArtDocument() call should return - art_bridge.h itself stays
// `extern "C"` (ABI-compatible with what ART's own codegen expects), so
// it can't take a real artisan::Node* parameter type. Included by
// main.cpp (the caller, once per navigate() - same place
// node_c_api_bridge.h's SetGoTimerContext is called) and art_bridge.cpp
// (the implementation).

namespace artisan {

class Node;
class TimerQueue;
class AnimationFrameQueue;

// nullptr while no document is live (between main.cpp's `document.reset()`
// and the next page finishing its build) - ArtDocument() returns that
// through unchanged, exactly the "no match" nullptr ArtIsNull already
// exists to test for.
void SetArtDocumentContext(Node *document);

// What ArtSetTimeout/ArtSetInterval/ArtRequestAnimationFrame (art_bridge.h)
// schedule into - a separate pair of globals from node_c_api_bridge.h's
// SetGoTimerContext (even though main.cpp's timerQueue/animationFrameQueue
// instances are the very same ones handed to both), mirroring
// SetArtDocumentContext's own "ART gets its own wiring, not routed through
// another binding's" precedent. Called once per navigate(), same place
// SetArtDocumentContext/SetGoTimerContext already are.
void SetArtTimerContext(TimerQueue &timers, AnimationFrameQueue &animationFrames);

} // namespace artisan
