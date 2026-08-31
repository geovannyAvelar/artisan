#pragma once

#include <string>
#include <vector>

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
// justify-content/align-items/align-content/flex-grow/flex-shrink/
// flex-basis/flex-wrap down to these) - kContainer only, same as the
// rest of the box model. flex-wrap has no effect in column direction
// (column's main axis - height - already has no fixed budget to wrap
// against in this flow model, same reason justify-content already
// degenerates to flex-start there - see RenderFlexContainer).
// align-content is likewise a no-op in column direction, for the same
// reason (it only has anything to redistribute among when flex-wrap
// actually produced more than one line) - and in row direction, it only
// has anything to redistribute at all when the container has an explicit
// CSS height taller than its wrapped lines' natural combined extent (see
// RenderFlexContainer): an auto-height container's cross size *is* its
// content's, with nothing left over, matching real CSS.
// Lives here rather than in css.h because css.h already includes this
// header (for Color, above) to avoid a circular include - not the other
// way around.
//
// CSS Grid (display: grid, below) is a second, independent layout mode
// alongside flexbox - kContainer only, same as flexbox, and a bounded
// subset in the same spirit: grid-template-columns/rows accept only a
// space-separated list of fixed px, `fr` (fractional), and min-content/
// max-content tracks (see GridTrack) - no repeat()/minmax()/auto/
// percentage tracks, no named lines. grid-template-areas/grid-area name
// cells; grid-column/grid-row place by numeric grid line instead (see
// GridLinePlacement) - either way, anything unplaced still auto-places
// into cells in document order, along whichever axis grid-auto-flow
// names (see GridAutoFlow, RenderGridContainer in widget_renderer.cpp).
// justify-items/align-items (reusing this same AlignItems enum and,
// for align-items, the very same Widget field flexbox's own cross-axis
// alignment already uses - see Widget::alignItems below) control
// whether an item stretches to fill its cell on each axis (the default,
// same as flexbox's own) or keeps its natural size and is start/center/
// end-positioned within it instead - no per-item justify-self/
// align-self, only these two container-level properties.
enum class DisplayMode { kBlock, kFlex, kGrid };
enum class FlexDirection { kRow, kColumn };
enum class JustifyContent { kFlexStart, kCenter, kFlexEnd, kSpaceBetween };
enum class AlignItems { kFlexStart, kCenter, kFlexEnd, kStretch };
// Real CSS's default for align-content is `stretch` (see AlignContent's
// use on Widget below for why it's fine for this enum's own default to
// differ from that).
enum class AlignContent {
  kFlexStart,
  kCenter,
  kFlexEnd,
  kSpaceBetween,
  kSpaceAround,
  kStretch,
};
enum class FlexWrap { kNowrap, kWrap };

// A single grid-template-columns/rows track's sizing function - real
// CSS's four most useful ones (see ParseGridTrackList, css.cpp; not
// repeat()/minmax()/auto/percentage tracks or named lines):
//   kFixed - a plain pixel size (`value`).
//   kFraction - an `Nfr` unit (`value` is the fr count) that shares
//     whatever space is left over after every fixed/min-content/
//     max-content track (and every inter-track gap) is subtracted,
//     proportionally to its own fr count relative to every other fr
//     track's - the same "distribute leftover space by weight" idea
//     flex-grow already uses for item sizing, just applied to track
//     sizing instead.
//   kMinContent - sized to the narrowest any single cell placed in this
//     track could be without overflowing (its widest unbreakable word,
//     for text) - columns only; a row track resolves this identically
//     to an ordinary auto-sized row (see RenderGridContainer's own doc
//     comment on why rows don't get an independently-meaningful
//     min/max-content of their own in this bounded subset).
//   kMaxContent - sized to the widest any single cell placed in this
//     track would be with no wrapping at all - same columns-only scope
//     as kMinContent above.
// `value` is unused for kMinContent/kMaxContent.
enum class GridTrackKind { kFixed, kFraction, kMinContent, kMaxContent };
struct GridTrack {
  GridTrackKind kind = GridTrackKind::kFixed;
  float value = 0.0f; // Pixels (kFixed) or the fr count (kFraction).
};

// A grid-column/grid-row value - real CSS's numeric, line-based
// placement (1-indexed grid lines - line 1 is before the first track,
// line 2 between the first and second, etc.), a bounded subset: an
// explicit start line and/or a span count, e.g. `grid-column: 2` (start
// line 2, span 1), `grid-column: 2 / 4` (start line 2, span 2 - the
// lines it spans between), `grid-column: span 2` (no explicit start,
// span 2), `grid-column: 2 / span 2` (start line 2, span 2). Real CSS's
// named lines and negative (counted-from-the-end) line numbers aren't
// supported - see ParseGridLinePlacement, css.cpp.
struct GridLinePlacement {
  bool hasStart = false;
  int start = 0; // 1-indexed grid line; only meaningful when hasStart.
  int span = 1;  // Always >= 1.
};

// grid-auto-flow - which axis auto-placement (RenderGridContainer,
// widget_renderer.cpp) fills first: kRow (the default) wraps to a new
// row once the current one runs out of columns, same as every grid
// before this property existed; kColumn wraps to a new column once the
// current one runs out of rows instead. Real CSS's `dense` packing
// modifier (re-filling earlier holes an explicit placement left open,
// rather than always moving forward) isn't supported - this engine's
// auto-placement has never tracked cell occupancy at all (see
// RenderGridContainer's own doc comment on the placement precedence
// ladder), and dense packing is meaningless without it; `grid-auto-flow:
// row dense`/`column dense` still resolve their own row/column keyword
// correctly, just without the dense behavior (see ParseDeclarations,
// css.cpp).
enum class GridAutoFlow { kRow, kColumn };

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

  // kBox, kLink, kLabel, and block-level kContainer: the Node this widget
  // was materialized from - lets a hit-test find which live DOM node the
  // user clicked (or is hovering), to focus/edit or click it (or, for
  // kLink, read its href and navigate; for kLabel, read its `for` and
  // activate the target it points at; a container has nothing to
  // activate, but still needs to be hit-testable so `:hover` styling on
  // an otherwise non-interactive element like a `<div>`/`<li>` works -
  // see ContainerWidgetHandler in widget_renderer.cpp). Opaque here so
  // widget.h doesn't need to know about Node.
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
  // - kContainer (widget_renderer.cpp's ContainerWidgetHandler), kBox
  // (BoxWidgetHandler), kLink, and kLabel (both via RenderTextLikeBox),
  // kTable/kTableCell (TableWidgetHandler - the table itself honors
  // margin/padding/width/height/background/border same as a kContainer,
  // a cell honors width/height/padding but not margin, which has no
  // effect on a real table-cell either), kRule (RuleWidgetHandler - no
  // padding, a <hr> has no content of its own to inset), and kImage
  // (ImageWidgetHandler - background/border only when both width and
  // height are already known ahead of decode, see its Render()) so far.
  // kText deliberately never will: MakeText (widget_tree_builder.cpp)
  // passes a bare text node its *parent's own* resolved Declarations
  // wholesale (text has no tag/class/id of its own to resolve a style
  // against), so giving kText a box model would double-apply the
  // parent's own margin/padding/width/etc onto its text - real CSS
  // doesn't give a bare text run an independent box model either.
  // Unlike Declarations, padding/margin carry no hasXxx flag here: by
  // the time BuildWidgetTree copies the cascade's resolved Declarations
  // onto a Widget, "not set by any rule" has already resolved to 0 -
  // exactly the inset WidgetRenderer should add for an unset side, no
  // ambiguity left to preserve. (BoxWidgetHandler's text kBox is the one
  // exception: it has its own non-zero intrinsic default padding, kept
  // as a rendering-time constant rather than a Widget field, that CSS
  // padding adds to rather than replaces - see its Render().) width/
  // height keep their hasXxx flag since 0 is never a substitute for "use
  // the natural/inherited size instead".
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
  // justifyContent/alignItems/alignContent/gap's own defaults (kRow/
  // kStretch/kFlexStart/0) are already the right value to fall back to
  // when a rule/inline style never mentioned them - same reasoning
  // padding/margin's Widget-level fields above don't need hasXxx either.
  bool hasDisplay = false;
  DisplayMode display = DisplayMode::kBlock;
  FlexDirection flexDirection = FlexDirection::kRow;
  JustifyContent justifyContent = JustifyContent::kFlexStart;
  // Real CSS flexbox's own default - also, unrelatedly, real CSS
  // Grid's own default for the exact same property when this Widget is
  // a grid container instead (RenderGridContainer, widget_renderer.cpp,
  // reads this field too - see ParseDeclarations, css.cpp, for why one
  // field serves both display modes).
  AlignItems alignItems = AlignItems::kStretch;
  // justify-items - CSS Grid only (RenderGridContainer); flexbox has no
  // equivalent property (justify-content distributes free space among
  // *items*, not each item within its own line - a different concept
  // from positioning one item within its own single cell, hence its own
  // field here rather than reusing justifyContent above).
  AlignItems justifyItems = AlignItems::kStretch;
  // kFlexStart, not real CSS's own kStretch default: kFlexStart is
  // exactly the "lines just stack contiguously" behavior this engine
  // already had before align-content existed at all, so a page that
  // never mentions the property (the overwhelming majority) keeps
  // rendering byte-for-byte the same - only a rule that explicitly sets
  // align-content now does anything different.
  AlignContent alignContent = AlignContent::kFlexStart;
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

  // CSS Grid - kContainer only, meaningful when hasDisplay/display is
  // kGrid (see DisplayMode above for this bounded subset's scope). No
  // hasXxx flag needed: an empty vector already means "unset", the
  // correct fallback RenderGridContainer treats as a single full-width
  // column / auto-sized rows - same reasoning gap doesn't need one
  // either.
  std::vector<GridTrack> gridTemplateColumns;
  std::vector<GridTrack> gridTemplateRows;

  // grid-template-columns: subgrid - columns only (grid-template-rows:
  // subgrid isn't supported; a subgrid row track resolves the same
  // "auto" way an unrecognized/fr row track already does, the same
  // columns-only asymmetry fr/min-content/max-content row tracks
  // already have, for the same reason: this engine's rows are always
  // content-auto-sized, with no independent per-row pixel budget of
  // their own to adopt anything into). gridTemplateColumnsSubgrid true
  // means gridTemplateColumns above is ignored entirely - the actual
  // track widths instead come from subgridColumnWidths, populated by
  // the *parent* grid container (RenderGridContainer,
  // widget_renderer.cpp) on a shallow copy of this Widget, right before
  // rendering it, with the slice of the parent's own already-resolved
  // column widths this item's placement spans - the same shallow-copy-
  // and-override pattern justify-items/align-items's own stretch
  // override already uses to hand a computed value down to a child
  // without mutating the shared tree. Left empty when this container
  // isn't actually a grid item of a compatible parent grid (or simply
  // isn't a subgrid at all), in which case a subgrid container falls
  // back to the same single-full-width-column behavior an ordinary
  // unset grid-template-columns already has - no special error
  // handling needed, since a Widget's own defaults already describe
  // exactly that fallback.
  bool gridTemplateColumnsSubgrid = false;
  std::vector<float> subgridColumnWidths;

  // grid-template-areas - one entry per row, each itself one area-name
  // token per column ("." meaning that cell is explicitly empty) - e.g.
  // `grid-template-areas: "header header" "sidebar content";` becomes
  // {{"header","header"}, {"sidebar","content"}}. Empty (the default)
  // means unset, same as gridTemplateColumns/Rows above. When set, this
  // - not gridTemplateColumns' own size - determines the grid's column/
  // row count (see RenderGridContainer): gridTemplateColumns/Rows still
  // size those columns/rows if their own count happens to match, but a
  // mismatched or unset gridTemplateColumns falls back to equal-width
  // columns instead of the single-full-width-column fallback that
  // applies without areas.
  std::vector<std::vector<std::string>> gridTemplateAreas;

  // grid-area - meaningful when this Widget is someone else's grid
  // child (read by the parent's RenderGridContainer, same "item
  // property in the same flat namespace" shape flexGrow/flexShrink
  // above already have) whose gridTemplateAreas names this string
  // somewhere - that name's bounding box (which may span multiple
  // cells - see RenderGridContainer) becomes this item's placement.
  // Empty (the default/unset) - or a name gridTemplateAreas doesn't
  // contain - falls back to ordinary row-major auto-placement, the same
  // as every item already gets without named areas at all. Only the
  // named-area form is supported, not real CSS's row-start/column-
  // start/row-end/column-end line-based shorthand - that's grid-column/
  // grid-row below instead, kept as separate fields since real CSS
  // keeps them separate properties too (grid-area is just a shorthand
  // that can also set them, which this parser doesn't support - see
  // ParseDeclarations, css.cpp).
  std::string gridArea;

  // grid-column/grid-row - see GridLinePlacement above. Item
  // properties, same "read by the parent's RenderGridContainer" shape
  // gridArea above already has. Precedence when more than one kind of
  // placement applies to the same item: a matching gridArea wins first;
  // otherwise, either of these having an explicit start line places the
  // item outright (the *other* axis, if it has no explicit start of its
  // own, defaults to line 1 rather than participating in auto-
  // placement - real CSS's own algorithm for a partially-explicit item
  // is a lot more elaborate than this bounded subset implements);
  // otherwise, a span with no explicit start on either axis still
  // auto-places (in document order, same as ever) but at that span
  // rather than always 1x1; otherwise, plain 1x1 auto-placement, same
  // as an item with none of this set at all. See RenderGridContainer,
  // widget_renderer.cpp, for exactly where each case is decided.
  GridLinePlacement gridColumn;
  GridLinePlacement gridRow;

  // grid-auto-flow - see GridAutoFlow above. Container property (unlike
  // gridArea/gridColumn/gridRow just above): it governs how *this*
  // container auto-places whichever of its own children fall through to
  // auto-placement, not anything about this Widget as someone else's
  // child.
  GridAutoFlow gridAutoFlow = GridAutoFlow::kRow;
};

} // namespace artisan
