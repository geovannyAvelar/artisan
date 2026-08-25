#pragma once

#include "dom_node.h"

#include <memory>

namespace artisan {

// Defined by the generated translation unit artisanc produces from the
// app's UI markup at build time (see src/compiler/main.cpp). Constructs a
// fresh Node tree that's a structural mirror of that markup - same tags,
// attributes, text, and nesting - with <img> file bytes already embedded
// as static byte arrays, so nothing here does any file I/O or parsing at
// runtime either.
//
// The tree this returns carries no rendering interpretation whatsoever:
// artisanc doesn't know a <div> is block-level or that colspan means
// anything. That happens once, uniformly, in BuildWidgetTree
// (widget_tree_builder.h) - the same function used for a Node tree built
// by hand or by a script at runtime. This is what unifies the two: there
// is exactly one place that turns "a Node tree" into "a paintable Widget
// tree", regardless of where the Node tree came from.
std::unique_ptr<Node> BuildCompiledDocument();

} // namespace artisan
