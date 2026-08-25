#pragma once

#include "renderer.h"
#include "widget.h"

#include <string>
#include <vector>

namespace artisan {

// A kBox's inner padding between its border and its text, on every side.
// Shared with callers that need to convert a screen click into a
// character offset (CharIndexAtX below) - they need to know where the
// text itself starts within the box, not just the box's outer rect.
constexpr float kBoxPadding = 8.0f;

// Returns the character offset into `text` closest to `relativeX` pixels
// from the text's left edge (i.e. already relative to where the text
// starts - past the box border and kBoxPadding, not the box's own x).
// Used both to place the caret/selection this renderer draws and, by a
// caller doing hit-testing, to turn a click into a cursor position in the
// same units BoxWidgetHandler laid the text out in. Clamped to
// [0, text.size()]; byte offsets, not codepoints (matches the rest of
// this renderer's ASCII-first text handling).
int CharIndexAtX(const IRenderer &renderer, const std::string &text,
                  float fontSize, float relativeX);

// The on-screen rect a kBox widget (input/button) ended up at during the
// last Render() call, plus the Widget's userData - for a tree built from
// a mutable Node (see widget_tree_builder.h), that's the Node* it came
// from. Lets a caller hit-test a click against the layout WidgetRenderer
// already computed, without re-implementing block-flow layout elsewhere.
struct BoxRegion {
  const void *userData;
  float x;
  float y;
  float width;
  float height;
};

// Paints a compiled Widget tree through an IRenderer using a simple top-to-
// bottom block layout: containers with blockSpacing get vertical margin,
// text leaves word-wrap to the viewport width. The tree may be baked into
// the binary at build time by artisanc, or materialized at runtime from a
// mutable Node tree (widget_tree_builder.h) - this class doesn't care
// which, and never parses markup itself either way.
class WidgetRenderer {
public:
  explicit WidgetRenderer(IRenderer &renderer);

  // If `outBoxRegions` is non-null, it's cleared and filled with every
  // kBox widget's final rect (in the same order rendered).
  void Render(const Widget &root, int viewportWidth,
              std::vector<BoxRegion> *outBoxRegions = nullptr);

  // Runs the same layout as Render() without painting, to find the total
  // height `root` would occupy at `viewportWidth`. Lets a caller size a
  // canvas tall enough to hold the whole document before painting it -
  // e.g. to render the full page once and let scrolling just move which
  // window into that surface gets shown, rather than laying out per
  // scroll position.
  float MeasureContentHeight(const Widget &root, int viewportWidth) const;

private:
  IRenderer &renderer_;
};

} // namespace artisan
