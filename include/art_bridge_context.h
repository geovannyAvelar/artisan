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

// nullptr while no document is live (between main.cpp's `document.reset()`
// and the next page finishing its build) - ArtDocument() returns that
// through unchanged, exactly the "no match" nullptr ArtIsNull already
// exists to test for.
void SetArtDocumentContext(Node *document);

} // namespace artisan
