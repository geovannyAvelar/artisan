#pragma once

#include "css.h"
#include "dom_node.h"
#include "widget.h"

#include <memory>
#include <string>
#include <vector>

namespace artisan {

// The font size every top-level element in a Node tree renders at unless
// a heading tag overrides it - shared here so a caller hit-testing a
// click against an <input>'s text (CharIndexAtX in widget_renderer.h)
// measures against the same size BuildWidgetTree actually used.
constexpr float kDefaultFontSize = 16.0f;

// Owns a Widget tree materialized from a mutable Node tree by
// BuildWidgetTree() below. Widget itself only ever holds raw, non-owning
// pointers (by design - it's meant to double as static const data baked
// into the binary by artisanc), so for a tree built at runtime instead,
// something has to own the backing storage those pointers point into.
// That's this class: every Widget array and every string BuildWidgetTree
// allocates lives here, and stays valid for as long as this WidgetTree
// does. WidgetTree itself is never moved or copied (only ever handled via
// unique_ptr), so those pointers stay valid regardless of what the caller
// does with the unique_ptr.
class WidgetTree {
public:
  const Widget &root() const { return root_; }
  void SetRoot(const Widget &root) { root_ = root; }

  // Keeps a Widget array (a node's `children`) or a string (a node's
  // `text`) alive for this tree's lifetime and returns a stable pointer
  // to it. Used by BuildWidgetTree's internals - exposed here rather than
  // made private + friended since several free functions need it, not
  // just BuildWidgetTree itself.
  const Widget *StoreArray(std::vector<Widget> widgets);
  const char *StoreString(const std::string &text);

private:
  std::vector<std::unique_ptr<std::vector<Widget>>> arrays_;
  std::vector<std::unique_ptr<std::string>> strings_;
  Widget root_{};
};

// Which <input> node (if any) currently has focus, and where its caret/
// selection sits - view-only state, kept separate from Node/the DOM the
// same way a browser's Selection isn't part of the DOM tree either.
struct InputFocus {
  Node *node = nullptr;
  int cursorPos = 0;
  int selectionAnchor = 0; // Equal to cursorPos means no selection.
};

// Walks `root`'s children and builds a Widget tree with the same shape
// artisanc would compile from equivalent markup - same tag classification
// (block/inline containers, headings, br/hr, input/button boxes), so
// WidgetRenderer can render either one without modification. Call this
// again whenever the Node tree changes; it's a one-shot snapshot, not a
// live view - there's no incremental update yet.
//
// Every kBox widget's `userData` is set to the Node it came from, so a
// hit-test against WidgetRenderer's BoxRegion output can recover which
// live node was clicked. `focus.node`, if set, must be an <input> node in
// this tree - that widget's cursorPos/selectionAnchor get set from
// `focus` so WidgetRenderer draws its caret/highlight; purely a rendering
// decision; the Node's actual `value` attribute is never touched by it.
// `pseudoState` is what `:hover`/`:focus` selectors in a `<style>` block
// match against while resolving each element's style (css.h) - pass the
// caller's actual live mouse/focus state to get real hover/focus
// styling, or omit it (default: nothing hovered or focused) for a
// one-shot build that shouldn't reflect either.
std::unique_ptr<WidgetTree> BuildWidgetTree(const Node &root,
                                             const InputFocus &focus = {},
                                             const PseudoClassState &pseudoState = {});

// An <input>'s `type` attribute picks checkbox/radio's small fixed-size
// indicator over the default single-line text box - anything else
// (including a missing `type`) keeps the text behavior. Exposed so a
// caller handling clicks (main.cpp) can make the same distinction
// BuildWidgetTree already makes when it decides which kind of kBox to
// build.
bool IsCheckableInputType(const Node &node);

} // namespace artisan
