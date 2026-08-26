#pragma once

namespace artisan {

// A plain RGB color - shared between Widget (what a resolved style bakes
// in) and IRenderer (what actually gets painted). No alpha: nothing here
// composites, so it would only ever be all-or-nothing, which named colors
// like "transparent" already approximate well enough without it.
struct Color {
  unsigned char r = 0;
  unsigned char g = 0;
  unsigned char b = 0;
};

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
  kLink,       // <a>: an underlined, single-line clickable label - the
               // same hit-testing shape as kBox (userData holds the
               // Node*), but painted without a border, since navigating
               // isn't "boxy" the way a button/input is.
  kLabel,      // <label>: same shape as kLink (flattened single-line
               // clickable text, userData holds the Node*) but painted
               // without an underline - see BoxKind::kCheckbox/kRadio
               // below for what a <label for="..."> actually activates.
};

// kBox only - what kind of box this is. kText is everything kBox already
// did before checkbox/radio existed (a single-line text box, sized to fit
// its label - <input> and <button> both still use this); kCheckbox/kRadio
// are a small fixed-size toggle indicator instead, with no label of their
// own (the human-readable label is a separate <label for="..."> element -
// see kLabel above).
enum class BoxKind { kText, kCheckbox, kRadio };

// A single node of a paintable UI tree - what WidgetRenderer actually
// walks. Always materialized at runtime by BuildWidgetTree
// (widget_tree_builder.h) from a Node tree, whether that Node tree was
// compiled from markup by artisanc or built by hand/script - Widget
// itself has no idea which, and doesn't need to.
struct Widget {
  WidgetKind kind;
  bool blockSpacing; // kContainer only: add vertical margin around children.
  float fontSize;     // kText, kLineBreak, kBox, kLink, kTable (caption).
  // kText, kBox, kLink (may be ""); kTable's optional <caption> text
  // (nullptr if none); nullptr otherwise (in particular, kTableCell - see
  // `children`).
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

  // kBox, kLink, kLabel only: the Node this widget was materialized from -
  // lets a hit-test find which live DOM node the user clicked, to
  // focus/edit or click it (or, for kLink, read its href and navigate; for
  // kLabel, read its `for` and activate the target it points at).
  // Opaque here so widget.h doesn't need to know about Node.
  const void *userData = nullptr;

  // kBox only, and only meaningful when this is the focused <input>: the
  // caret's character offset into `text`. -1 means "not focused, don't
  // draw a caret" - the vast majority of boxes. When selectionAnchor is
  // also >= 0 and different from cursorPos, the renderer highlights the
  // range between them instead of just drawing a caret line.
  int cursorPos = -1;
  int selectionAnchor = -1;

  // kBox only. See BoxKind above; boxKind stays kText for every kBox this
  // struct held before checkbox/radio existed. checked only means
  // anything when boxKind != kText - whether that indicator paints
  // filled, from the source <input>'s `checked` attribute.
  BoxKind boxKind = BoxKind::kText;
  bool checked = false;

  // CSS (css.h), resolved once at widget-build time - see
  // widget_tree_builder.cpp. color/bold inherit to descendants that don't
  // set their own (same as real CSS); backgroundColor/borderColor/
  // borderWidth don't. hasXxx distinguishes "not set" from a resolved
  // value that happens to be black/1px - which kinds actually paint these
  // is up to WidgetRenderer, not this struct.
  bool hasColor = false;
  Color color;
  bool bold = false;
  bool hasBackgroundColor = false;
  Color backgroundColor;
  bool hasBorderColor = false;
  Color borderColor;
  float borderWidth = 1.0f;
};

} // namespace artisan
