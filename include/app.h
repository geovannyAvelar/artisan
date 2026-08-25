#pragma once

#include "dom_node.h"

namespace artisan {

// Implemented in ordinary C++ (see app.cpp) and compiled straight into
// the binary - no markup, no script, no interpreter, just plain function
// calls against the Node API. Called once at startup, after the document
// is built from compiled markup and before the event loop starts, so it
// can find nodes (Node::FindById) and wire up behavior (SetOnClick,
// mutate attributes/text, even build and append new nodes) exactly like
// a script would - just ahead-of-time compiled instead of interpreted.
//
// This is the native counterpart to JsEngine (js_engine.h): both operate
// on the same Node tree through the same public API, so "should this bit
// of behavior be native C++ or script" is a per-feature choice, not an
// architectural one - main.cpp runs both, back to back, against the one
// document.
void SetupApp(Node &document);

} // namespace artisan
