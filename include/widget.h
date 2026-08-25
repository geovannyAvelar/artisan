#pragma once

namespace artisan {

enum class WidgetKind {
  kContainer,  // Groups children (div, span, body, ...); no text of its own.
  kText,       // A leaf that paints `text` at `fontSize` (word-wrapped).
  kLineBreak,  // <br>: forces a line break, nothing to paint.
  kRule,       // <hr>: a horizontal bar spanning the layout width.
  kBox,        // <input>/<button>: a bordered box with a single-line label.
  kImage,      // <img>: paints pre-loaded, embedded image bytes.
  kTable,      // <table>: children are rows (plain kContainer of cells);
               // laid out as a grid with per-column widths, not flowed.
  kTableCell,  // <td>/<th>: children are the cell's own nested content,
               // laid out (and wrapped) within whatever width the grid
               // gives its column - not flattened to a single line.
};

// A single node of a paintable UI tree - what WidgetRenderer actually
// walks. Always materialized at runtime by BuildWidgetTree
// (widget_tree_builder.h) from a Node tree, whether that Node tree was
// compiled from markup by artisanc or built by hand/script - Widget
// itself has no idea which, and doesn't need to.
struct Widget {
  WidgetKind kind;
  bool blockSpacing; // kContainer only: add vertical margin around children.
  float fontSize;     // kText, kLineBreak, kBox, kTable (caption).
  // kText, kBox (may be ""); kTable's optional <caption> text (nullptr if
  // none); nullptr otherwise (in particular, kTableCell - see `children`).
  const char *text;
  // kContainer, kTable (rows), kTableCell (nested content); nullptr
  // otherwise.
  const Widget *children;
  int childCount;

  // kImage only: the raw encoded (PNG/JPEG/GIF/WebP) file bytes, embedded
  // into the binary by artisanc from the <img src="..."> file at compile
  // time. Decoded at runtime by IRenderer - decoding pixels from an
  // already-compiled-in byte blob is a rendering concern, not markup
  // interpretation, same as turning font outlines into glyphs is.
  const unsigned char *imageData;
  int imageDataSize;

  // kImage only: explicit size from the `width`/`height` attributes, 0
  // meaning unspecified. If only one is set the other is derived from the
  // decoded image's aspect ratio at render time; if neither is set the
  // image is scaled to fit the layout width, preserving aspect ratio.
  //
  // imageWidth holds a plain pixel value unless imageWidthIsPercent is
  // set, in which case it holds a percentage (e.g. 50 for "50%") to
  // resolve against the layout width at render time - percentages can't
  // be resolved at compile time since the viewport width isn't known yet.
  // imageHeight is always a pixel value: a percentage height has no
  // containing-block height to resolve against in this layout model, so
  // artisanc rejects it.
  float imageWidth;
  float imageHeight;
  bool imageWidthIsPercent;

  // kTableCell only: how many grid columns/rows this cell spans, from the
  // colspan/rowspan attributes. Always >= 1 - a missing or invalid
  // attribute means 1, not 0; artisanc normalizes this before emitting.
  int colSpan;
  int rowSpan;

  // kBox only: the Node this widget was materialized from - lets a
  // hit-test find which live DOM node the user clicked, to focus/edit or
  // click it. Opaque here so widget.h doesn't need to know about Node.
  const void *userData = nullptr;

  // kBox only, and only meaningful when this is the focused <input>: the
  // caret's character offset into `text`. -1 means "not focused, don't
  // draw a caret" - the vast majority of boxes. When selectionAnchor is
  // also >= 0 and different from cursorPos, the renderer highlights the
  // range between them instead of just drawing a caret line.
  int cursorPos = -1;
  int selectionAnchor = -1;
};

} // namespace artisan
