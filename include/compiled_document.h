#pragma once

#include "dom_node.h"

#include <memory>
#include <string>
#include <vector>

namespace artisan {

// One page of a compiled bundle: a name (derived by artisanc from the
// markup file's stem - "settings.html" becomes "settings") and the
// function that builds a fresh Node tree for it. <a href="..."> navigates
// by looking its href up against these names (see main.cpp's Navigate()).
struct PageDescriptor {
  std::string name;
  std::unique_ptr<Node> (*build)();
};

// Defined by the generated translation unit artisanc produces from the
// app's UI markup at build time (see src/compiler/main.cpp) - one entry
// per markup file the build was configured with (ARTISAN_UI_SOURCES /
// repeatable --html), in the order given; pages[0] is the one the app
// opens on. Each page's Node tree is a structural mirror of its markup -
// same tags, attributes, text, and nesting - with <img> file bytes
// already embedded as static byte arrays, so nothing here does any file
// I/O or parsing at runtime either.
//
// The trees this returns carry no rendering interpretation whatsoever:
// artisanc doesn't know a <div> is block-level or that colspan means
// anything. That happens once, uniformly, in BuildWidgetTree
// (widget_tree_builder.h) - the same function used for a Node tree built
// by hand or by a script at runtime. This is what unifies the two: there
// is exactly one place that turns "a Node tree" into "a paintable Widget
// tree", regardless of where the Node tree came from.
const std::vector<PageDescriptor> &CompiledPages();

} // namespace artisan
