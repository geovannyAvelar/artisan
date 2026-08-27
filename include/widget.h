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

// Flexbox (see widget_renderer.cpp's RenderFlexContainer, and css.h's
// Declarations, which resolves markup's display/flex-direction/
// justify-content/align-items/flex-grow/flex-shrink/flex-basis/
// flex-wrap down to these) - kContainer only, same as the rest of the
// box model. No CSS Grid, no align-content (a wrapped flex container's
// multiple lines just stack in DOM order along the cross axis, no extra
// distribution control over them) and flex-wrap has no effect in column
// direction (column's main axis - height - already has no fixed budget
// to wrap against in this flow model, same reason justify-content
// already degenerates to flex-start there - see RenderFlexContainer).
// Lives here rather than in css.h because css.h already includes this
// header (for Color, above) to avoid a circular include - not the other
// way around.
enum class DisplayMode { kBlock, kFlex };
enum class FlexDirection { kRow, kColumn };
enum class JustifyContent { kFlexStart, kCenter, kFlexEnd, kSpaceBetween };
enum class AlignItems { kFlexStart, kCenter, kFlexEnd, kStretch };
enum class FlexWrap { kNowrap, kWrap };

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

  // Box model (css.h's Declarations, same widget-build-time resolution)
  // - kContainer only for now (see widget_renderer.cpp's
  // ContainerWidgetHandler). Unlike Declarations, padding/margin carry
  // no hasXxx flag here: by the time BuildWidgetTree copies the
  // cascade's resolved Declarations onto a Widget, "not set by any
  // rule" has already resolved to 0 - exactly the inset WidgetRenderer
  // should add for an unset side, no ambiguity left to preserve.
  // width/height keep their hasXxx flag since 0 is never a substitute
  // for "use the natural/inherited size instead".
  bool hasWidth = false;
  float width = 0.0f;
  bool widthIsPercent = false;
  bool hasHeight = false;
  float height = 0.0f;
  float paddingTop = 0.0f;
  float paddingRight = 0.0f;
  float paddingBottom = 0.0f;
  float paddingLeft = 0.0f;
  float marginTop = 0.0f;
  float marginRight = 0.0f;
  float marginBottom = 0.0f;
  float marginLeft = 0.0f;

  // Flexbox - kContainer only. Only hasDisplay/display need a presence
  // flag (whether display:flex is even active); flexDirection/
  // justifyContent/alignItems/gap's own defaults (kRow/kStretch/0) are
  // already the right value to fall back to when a rule/inline style
  // never mentioned them - same reasoning padding/margin's Widget-level
  // fields above don't need hasXxx either.
  bool hasDisplay = false;
  DisplayMode display = DisplayMode::kBlock;
  FlexDirection flexDirection = FlexDirection::kRow;
  JustifyContent justifyContent = JustifyContent::kFlexStart;
  AlignItems alignItems = AlignItems::kStretch; // Real CSS flexbox's own default.
  float gap = 0.0f;
  FlexWrap flexWrap = FlexWrap::kNowrap;

  // Flex *item* properties - meaningful when this Widget is someone
  // else's flex child, read by the parent's RenderFlexContainer, not by
  // this widget's own Render() (same flat-namespace shape every other
  // Widget field already has - nothing here distinguishes "container"
  // fields from "item" fields structurally, since any Widget can be
  // either depending on who its parent is). Defaults (grow=0, shrink=1)
  // are real CSS flexbox's own defaults - items don't grow but do
  // shrink to fit unless told otherwise.
  float flexGrow = 0.0f;
  float flexShrink = 1.0f;
  bool hasFlexBasis = false;
  float flexBasis = 0.0f;
};

} // namespace artisan
