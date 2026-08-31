#include "widget_renderer.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace artisan {

namespace {

constexpr float kMargin = 20.0f;
constexpr float kBlockSpacing = 14.0f;
constexpr float kRuleHeight = 2.0f;
constexpr float kBoxMinWidth = 120.0f;
constexpr float kCellPadding = 6.0f;
constexpr float kCellMinWidth = 24.0f;
constexpr float kCellMinHeight = 20.0f;
constexpr float kCaretWidth = 1.5f;
constexpr float kLinkUnderlineHeight = 1.5f;
// kCheckbox/kRadio's fixed indicator size (a checkbox's side length, a
// radio's diameter) and how far the filled checked-mark insets from that
// outer edge - unlike a text kBox, neither scales with fontSize/label
// text, since neither has a label of its own (see BoxKind in widget.h).
constexpr float kCheckableSize = 16.0f;
constexpr float kCheckableInset = 4.0f;

struct LayoutState {
  IRenderer &renderer;
  float x;
  float y;
  float maxWidth;

  // Inline text (kText leaves, plus whatever inline containers like <span>
  // flow through) accumulates here across sibling widgets so consecutive
  // inline content shares real lines instead of each widget starting its
  // own. Block-level boundaries flush it before moving on.
  std::string pendingLine;
  float pendingFontSize = 0.0f;
  bool pendingBold = false;
  Color pendingColor = kDefaultTextColor;

  // Non-null only on the real (non-dry-run) top-level render: where
  // BoxWidgetHandler records each box's final on-screen rect for hit
  // testing. Left null for the table measurement/dry-run passes - their
  // coordinates are cell-relative, not real screen positions.
  std::vector<BoxRegion> *boxRegions = nullptr;

  // Set (on a widget's own fresh, per-item LayoutState) by
  // RenderFlexContainer below, right before rendering one flex item -
  // every WidgetKind's handler normally adds kBlockSpacing before/after
  // its own box (spacing between ordinary block-flow siblings), but a
  // flex item is spaced from its siblings by `gap` instead, so that
  // widget's *own* handler needs to skip it just this once. See
  // ConsumeSuppressBlockSpacing.
  bool suppressBlockSpacing = false;
};

// Reads and resets suppressBlockSpacing - every handler that adds
// kBlockSpacing calls this once, at the very top of its own Render(),
// so the suppression only ever applies to that one widget's own outer
// spacing, never propagating to whatever it goes on to recurse into
// (e.g. a flex item that's itself an ordinary block container still
// spaces *its own* children normally - only its own top/bottom spacing,
// relative to its flex siblings, is suppressed).
bool ConsumeSuppressBlockSpacing(LayoutState &state) {
  bool suppress = state.suppressBlockSpacing;
  state.suppressBlockSpacing = false;
  return suppress;
}

void FlushLine(LayoutState &state) {
  if (state.pendingLine.empty()) {
    return;
  }

  state.renderer.DrawText(state.pendingLine, state.x, state.y,
                           state.pendingFontSize, state.pendingBold,
                           state.pendingColor);
  state.y += state.renderer.LineHeight(state.pendingFontSize, state.pendingBold);
  state.pendingLine.clear();
}

void AppendWrappedText(LayoutState &state, const std::string &text,
                        float fontSize, bool bold, const Color &color) {
  // A font-size/weight/color change (e.g. a heading right after inline
  // text, or a <span style="color:..."> mid-paragraph) can't share a line
  // with what came before - this flow model draws one line with one set
  // of text attributes, not per-run styling within a line.
  if (!state.pendingLine.empty() &&
      (state.pendingFontSize != fontSize || state.pendingBold != bold ||
       state.pendingColor.r != color.r || state.pendingColor.g != color.g ||
       state.pendingColor.b != color.b)) {
    FlushLine(state);
  }
  state.pendingFontSize = fontSize;
  state.pendingBold = bold;
  state.pendingColor = color;

  std::istringstream words(text);
  std::string word;

  while (words >> word) {
    std::string candidate =
        state.pendingLine.empty() ? word : state.pendingLine + " " + word;
    float width = state.renderer.MeasureText(candidate, fontSize, bold);

    if (width > state.maxWidth && !state.pendingLine.empty()) {
      state.renderer.DrawText(state.pendingLine, state.x, state.y, fontSize,
                               bold, color);
      state.y += state.renderer.LineHeight(fontSize, bold);
      state.pendingLine = word;
    } else {
      state.pendingLine = candidate;
    }
  }
}

// Dispatches a single Widget to whichever WidgetHandler owns its kind -
// forward-declared so ContainerWidgetHandler can recurse into children
// through it.
void RenderWidget(const Widget &widget, LayoutState &state);

// An IRenderer that measures (via the real renderer's font metrics) but
// never paints. Table cells can now hold arbitrary nested content (not
// just flattened text), so sizing a cell means actually running the same
// block-flow layout used everywhere else and seeing how wide/tall it
// comes out - this lets that "dry run" reuse RenderWidget verbatim
// instead of a second, parallel measurement implementation.
class NullRenderer final : public IRenderer {
public:
  explicit NullRenderer(const IRenderer &inner) : inner_(inner) {}

  void DrawText(const std::string &text, float x, float /*y*/,
                float fontSize, bool bold, const Color & /*color*/) override {
    naturalWidth_ =
        std::max(naturalWidth_, x + inner_.MeasureText(text, fontSize, bold));
  }

  float MeasureText(const std::string &text, float fontSize,
                     bool bold) const override {
    return inner_.MeasureText(text, fontSize, bold);
  }

  float LineHeight(float fontSize, bool bold) const override {
    return inner_.LineHeight(fontSize, bold);
  }

  void DrawRect(float x, float /*y*/, float width, float /*height*/,
                float /*strokeWidth*/, const Color & /*color*/) override {
    naturalWidth_ = std::max(naturalWidth_, x + width);
  }

  void DrawFilledRect(float x, float /*y*/, float width, float /*height*/,
                       const Color & /*color*/) override {
    naturalWidth_ = std::max(naturalWidth_, x + width);
  }

  void DrawCircle(float cx, float /*cy*/, float radius,
                   float /*strokeWidth*/, const Color & /*color*/) override {
    naturalWidth_ = std::max(naturalWidth_, cx + radius);
  }

  void DrawFilledCircle(float cx, float /*cy*/, float radius,
                         const Color & /*color*/) override {
    naturalWidth_ = std::max(naturalWidth_, cx + radius);
  }

  // Real image dimensions are only known after decoding, which is a
  // rendering-backend concern this dry run deliberately skips - an image
  // inside a table cell contributes no size to a measurement pass. Rare
  // in practice and a reasonable simplification given how much table
  // layout already has to approximate.
  float DrawImage(const unsigned char * /*data*/, int /*dataSize*/,
                   float /*x*/, float /*y*/, float /*maxWidth*/,
                   float /*explicitWidth*/, float /*explicitHeight*/) override {
    return 0.0f;
  }

  float naturalWidth() const { return naturalWidth_; }

private:
  const IRenderer &inner_;
  float naturalWidth_ = 0.0f;
};

// Runs `widget`'s children through the real block-flow layout on a null
// renderer to find their natural (unwrapped, up to `cap`) width - the
// widest line any of them would draw. Used to size table columns before
// any actual column width is known.
float MeasureNaturalWidth(const Widget &widget, const IRenderer &renderer,
                           float cap) {
  NullRenderer nullRenderer(renderer);
  LayoutState dryRun{nullRenderer, 0.0f, 0.0f, cap};
  for (int i = 0; i < widget.childCount; ++i) {
    RenderWidget(widget.children[i], dryRun);
  }
  FlushLine(dryRun);
  return nullRenderer.naturalWidth();
}

// Same idea, but constrained to a specific width - how tall would this
// content actually come out once wrapped to fit a column of that width.
float MeasureHeightAt(const Widget &widget, const IRenderer &renderer,
                       float width) {
  NullRenderer nullRenderer(renderer);
  LayoutState dryRun{nullRenderer, 0.0f, 0.0f, width};
  for (int i = 0; i < widget.childCount; ++i) {
    RenderWidget(widget.children[i], dryRun);
  }
  FlushLine(dryRun);
  return dryRun.y;
}

// The real (painting) counterpart of the two measurement passes above -
// lays `widget`'s children out at (x, y) constrained to `width` and
// actually draws them.
void RenderContentAt(const Widget &widget, IRenderer &renderer, float x,
                      float y, float width) {
  LayoutState state{renderer, x, y, width};
  for (int i = 0; i < widget.childCount; ++i) {
    RenderWidget(widget.children[i], state);
  }
  FlushLine(state);
}

// Like MeasureNaturalWidth/MeasureHeightAt above, but measures `widget`
// itself as a single item (dispatching it through RenderWidget exactly
// once), not its children collectively - what a flex item needs (see
// RenderFlexContainer below): a flex child can be any widget kind (a
// <div>, but just as easily a <span>/<p>/<button>/kText leaf), not
// reliably a container whose own content lives in `.children` the way a
// kTableCell always is (which is what lets TableWidgetHandler get away
// with MeasureNaturalWidth/MeasureHeightAt directly on each cell).
float MeasureItemNaturalWidth(const Widget &widget, const IRenderer &renderer,
                               float cap) {
  NullRenderer nullRenderer(renderer);
  LayoutState dryRun{nullRenderer, 0.0f, 0.0f, cap};
  // Measuring a flex item's *natural* size must see the same suppressed
  // spacing the real paint call will use (see RenderFlexContainer) -
  // otherwise every item's measured size would include a phantom extra
  // kBlockSpacing above and below it that never actually renders,
  // inflating the whole container's cross size for nothing.
  dryRun.suppressBlockSpacing = true;
  RenderWidget(widget, dryRun);
  FlushLine(dryRun);
  return nullRenderer.naturalWidth();
}

// A flex item's natural main-size, for flex-basis/cross-size purposes -
// unlike MeasureItemNaturalWidth above (which renders the item as one
// unit, background/border box included), a kContainer item with no
// explicit width of its own would report that box as spanning however
// wide `cap` happens to be: real CSS's own "a block fills its
// container" default, exactly right for normal block-flow rendering
// (see ContainerWidgetHandler::Render's boxMaxWidth) but not what
// "natural width" means for an *unstretched flex item* - one of those
// wants its *content's* preferred width instead, the same way a real
// browser auto-sizes a flex item to fit-content rather than 100%. For a
// kContainer, that means measuring its own children directly
// (MeasureNaturalWidth, bypassing its own background/border box
// entirely, straight to whatever text/grandchildren actually determine
// a size) instead of treating the container as a single rendered unit.
// Every other kind's own Render() already computes a genuinely
// content-driven size on its own (e.g. BoxWidgetHandler's
// textWidth-based width), so MeasureItemNaturalWidth is already correct
// for those, unchanged.
float MeasureFlexItemNaturalWidth(const Widget &widget, const IRenderer &renderer,
                                   float cap) {
  if (widget.kind == WidgetKind::kContainer) {
    return MeasureNaturalWidth(widget, renderer, cap);
  }
  return MeasureItemNaturalWidth(widget, renderer, cap);
}

float MeasureItemHeightAt(const Widget &widget, const IRenderer &renderer,
                           float width) {
  NullRenderer nullRenderer(renderer);
  LayoutState dryRun{nullRenderer, 0.0f, 0.0f, width};
  dryRun.suppressBlockSpacing = true; // See MeasureItemNaturalWidth above.
  RenderWidget(widget, dryRun);
  FlushLine(dryRun);
  return dryRun.y;
}

// One flex line's own cross-axis extent, and the main-axis space its
// items+gaps actually occupied after flex-grow/flex-shrink resolved
// their final sizes - see LayoutFlexLine/RenderFlexContainer below for
// what each is used for.
struct FlexLineResult {
  float crossSize;
  float mainExtent;
};

// Lays out and paints one "flex line" - every item, when
// flex-wrap:nowrap (the only option in column direction - see
// RenderFlexContainer); one row's worth of items when
// flex-wrap:wrap groups a row container's items across several. Resolves
// flex-grow/flex-shrink against `basis` (each item's starting main size)
// to get final main sizes, then justify-content/align-items exactly as
// this function did before flex-grow/shrink/wrap existed.
// `crossAxisOffset` positions this line within the container's own
// cross axis - 0 for the only line there is, unless flex-wrap:wrap
// produced more than one (each subsequent line's own crossAxisOffset
// factors in every earlier line's own FlexLineResult::crossSize, plus
// whatever gap align-content put between them - see the caller).
// `minCrossSize` is align-content:stretch's doing (row only; see
// RenderFlexContainer) - this line's natural cross size (the tallest
// item, same as ever) still gets computed as usual below, but then
// widened to at least this if it's bigger, so align-items:stretch (which
// already stretches every item to fill lineCrossSize) picks up the
// larger, stretched size for free without needing its own separate
// logic. 0 for every align-content value other than stretch.
FlexLineResult LayoutFlexLine(const Widget &widget, LayoutState &state, bool isRow,
                               int startIndex, int count,
                               const std::vector<float> &renderWidth,
                               const std::vector<float> &basis,
                               float mainAxisAvailable, float crossAxisOffset,
                               float minCrossSize) {
  float gap = widget.gap;
  int n = count;

  std::vector<float> mainSize(n);
  std::vector<float> crossSize(n);
  for (int i = 0; i < n; ++i) {
    int idx = startIndex + i;
    mainSize[i] = basis[idx];
    crossSize[i] = isRow
                       ? MeasureItemHeightAt(widget.children[idx], state.renderer, renderWidth[idx])
                       : renderWidth[idx];
  }

  float totalBasis = static_cast<float>(n - 1) * gap;
  for (float m : mainSize) {
    totalBasis += m;
  }

  // Row always has a fixed main-axis budget (mainAxisAvailable); column's
  // main axis (height) is intrinsic - a caller-supplied explicit CSS
  // height (Part 2's box model) is only resolved by
  // ContainerWidgetHandler::Render *after* RenderFlexContainer returns,
  // so a column flex container's justify-content/flex-grow/flex-shrink
  // have no fixed budget to distribute against and are all no-ops there
  // (a documented interaction between Part 2 and Part 3, not a bug).
  float freeSpace = isRow ? mainAxisAvailable - totalBasis : 0.0f;

  // Resolve flex-grow/flex-shrink against basis - a single pass is
  // exact here (no iterative min/max-width clamping needed, since this
  // engine has no min-width/max-width property to clamp against - real
  // CSS's own algorithm only needs to iterate because of that).
  if (isRow && freeSpace > 0.0f) {
    float totalGrow = 0.0f;
    for (int i = 0; i < n; ++i) {
      totalGrow += widget.children[startIndex + i].flexGrow;
    }
    if (totalGrow > 0.0f) {
      for (int i = 0; i < n; ++i) {
        mainSize[i] += freeSpace * (widget.children[startIndex + i].flexGrow / totalGrow);
      }
      freeSpace = 0.0f; // Fully absorbed by growth - none left for justify-content.
    }
  } else if (isRow && freeSpace < 0.0f) {
    float totalShrinkWeighted = 0.0f;
    for (int i = 0; i < n; ++i) {
      totalShrinkWeighted += widget.children[startIndex + i].flexShrink * mainSize[i];
    }
    if (totalShrinkWeighted > 0.0f) {
      float deficit = -freeSpace;
      for (int i = 0; i < n; ++i) {
        float weight = widget.children[startIndex + i].flexShrink * mainSize[i];
        mainSize[i] = std::max(0.0f, mainSize[i] - deficit * (weight / totalShrinkWeighted));
      }
      freeSpace = 0.0f; // Fully absorbed by shrinking.
    }
    // Else: nothing can shrink (every item's flex-shrink*basis is 0) -
    // items simply overflow mainAxisAvailable, same as real CSS without
    // a shrinkable item either.
  }

  float leadingOffset = 0.0f;
  float betweenGap = gap;
  if (isRow) {
    switch (widget.justifyContent) {
    case JustifyContent::kFlexStart:
      break;
    case JustifyContent::kCenter:
      leadingOffset = freeSpace / 2.0f;
      break;
    case JustifyContent::kFlexEnd:
      leadingOffset = freeSpace;
      break;
    case JustifyContent::kSpaceBetween:
      if (n > 1) {
        betweenGap = gap + freeSpace / static_cast<float>(n - 1);
      }
      break;
    }
  }

  // This line's own cross-axis size: row's cross axis (height) is
  // intrinsic - the tallest item (stretch grows every item to that same
  // height, so it can't change the max), or minCrossSize if
  // align-content:stretch says this line should be taller than that;
  // column's cross axis (width) is simply the space available.
  float lineCrossSize;
  if (isRow) {
    lineCrossSize = 0.0f;
    for (float c : crossSize) {
      lineCrossSize = std::max(lineCrossSize, c);
    }
    lineCrossSize = std::max(lineCrossSize, minCrossSize);
  } else {
    lineCrossSize = state.maxWidth;
  }

  float mainCursor = leadingOffset;
  for (int i = 0; i < n; ++i) {
    int idx = startIndex + i;
    float itemMain = mainSize[i];
    float itemCross = crossSize[i];
    // renderWidth[idx] already prioritizes the item's own explicit
    // width over its measured natural/content width (see
    // MeasureFlexItemNaturalWidth and RenderFlexContainer's own
    // renderWidth computation) - the right default main-axis (row) or
    // cross-axis (column) render width before any grow/shrink/stretch
    // below has a chance to override it.
    float itemRenderWidth = renderWidth[idx];
    // A cheap shallow copy (Widget owns no resources - same POD-like
    // shape its own doc comment describes) - grow/shrink and stretch,
    // when either applies, impose a size the child doesn't have of its
    // own by overriding hasWidth/hasHeight on this copy before
    // rendering it, rather than mutating the real (const, possibly
    // shared/static) tree. The two never conflict: grow/shrink only
    // ever touches the *main*-axis field (width for row, height for
    // column); stretch only ever touches the *cross*-axis one.
    Widget effectiveChild = widget.children[idx];

    if (isRow) {
      // Compared against renderWidth[idx] (what would render *without*
      // any flex resolution - the item's own width, or its natural size
      // absent one), not basis[idx]: those two can already differ
      // before grow/shrink ever runs, whenever flex-basis overrides an
      // item's width outright (basis takes flex-basis; renderWidth
      // never does - see RenderFlexContainer) - comparing against basis
      // would miss exactly that case, since itemMain starts equal to
      // basis by construction and grow/shrink might never touch it.
      if (std::abs(itemMain - renderWidth[idx]) > 0.01f) {
        // Width affects wrapping, so the item needs re-measuring at its
        // new width, not just a bigger/smaller box around the same text
        // layout.
        effectiveChild.hasWidth = true;
        effectiveChild.widthIsPercent = false;
        effectiveChild.width = itemMain;
        itemRenderWidth = itemMain;
        itemCross = MeasureItemHeightAt(effectiveChild, state.renderer, itemRenderWidth);
      }
    } else if (std::abs(itemMain - basis[idx]) > 0.01f) {
      // Column: main axis is height, which (unlike width) has no
      // separate "renderHeight" default to diverge from basis the way
      // renderWidth can for row - itemMain already starts at basis[idx]
      // unconditionally (see mainSize[i] above), so this only ever
      // fires if something between basis and here changed it (nothing
      // does yet, column never grows/shrinks - kept for symmetry and in
      // case that changes). No re-measurement needed either way: height
      // (unlike width) doesn't affect anything else in this model, so
      // just an explicit height override, same mechanism
      // align-items:stretch already uses for a row item's height below.
      effectiveChild.hasHeight = true;
      effectiveChild.height = itemMain;
    }

    // Real CSS: stretch only takes effect when the item has no explicit
    // cross-size of its own - one that does keeps its own size, and
    // (this v1's simplification) is positioned as if flex-start rather
    // than getting center/flex-end's usual offset treatment too.
    if (widget.alignItems == AlignItems::kStretch) {
      if (isRow && !effectiveChild.hasHeight) {
        effectiveChild.hasHeight = true;
        effectiveChild.height = lineCrossSize;
        itemCross = lineCrossSize;
      } else if (!isRow && !effectiveChild.hasWidth) {
        // A container child with no explicit width of its own already
        // fills whatever maxWidth its own LayoutState gets (the same
        // "block children fill their container by default" rule this
        // renderer already has), so just handing it lineCrossSize as
        // childState's maxWidth below achieves stretch with no widget
        // mutation needed - but height DOES depend on width, so it
        // needs re-measuring at that width, unlike the row case above.
        itemRenderWidth = lineCrossSize;
        itemCross = lineCrossSize;
        itemMain = MeasureItemHeightAt(effectiveChild, state.renderer, itemRenderWidth);
      }
    }

    float crossOffset = 0.0f;
    if (widget.alignItems == AlignItems::kCenter) {
      crossOffset = (lineCrossSize - itemCross) / 2.0f;
    } else if (widget.alignItems == AlignItems::kFlexEnd) {
      crossOffset = lineCrossSize - itemCross;
    }

    float childX = isRow ? state.x + mainCursor : state.x + crossOffset;
    float childY = isRow ? state.y + crossAxisOffset + crossOffset : state.y + mainCursor;

    // A fresh LayoutState per item - flex items are always individually
    // positioned boxes, never inline-flowed text sharing one line the
    // way ordinary block-flow siblings can be. boxRegions must be
    // propagated explicitly: LayoutState's own default is nullptr (see
    // RenderContentAt above, which does *not* propagate it - a
    // pre-existing gap for table cells, not repeated here).
    LayoutState childState{state.renderer, childX, childY, itemRenderWidth};
    childState.boxRegions = state.boxRegions;
    childState.suppressBlockSpacing = true;
    RenderWidget(effectiveChild, childState);
    FlushLine(childState);

    mainCursor += itemMain + betweenGap;
  }
  mainCursor -= betweenGap; // Undo the trailing gap added after the last item.

  return {lineCrossSize, mainCursor};
}

// A flex line's natural (pre-stretch) cross size, without laying out or
// painting anything - the same "tallest item" computation LayoutFlexLine
// does internally for row (factored out here so RenderFlexContainer can
// learn every line's size *before* committing to any of their positions,
// which align-content needs: it has to know the total cross-axis extent
// every line's content would naturally take up before it can decide how
// to distribute whatever's left over). Column's is always state.maxWidth
// (the available width - see LayoutFlexLine's identical column case),
// though RenderFlexContainer never actually calls this for column, since
// column never wraps into more than one line for align-content to have
// anything to distribute among in the first place.
float MeasureLineCrossSize(const Widget &widget, LayoutState &state, bool isRow,
                            int startIndex, int count,
                            const std::vector<float> &renderWidth) {
  if (!isRow) {
    return state.maxWidth;
  }
  float lineCrossSize = 0.0f;
  for (int i = 0; i < count; ++i) {
    int idx = startIndex + i;
    lineCrossSize = std::max(
        lineCrossSize,
        MeasureItemHeightAt(widget.children[idx], state.renderer, renderWidth[idx]));
  }
  return lineCrossSize;
}

// A flexbox: display:flex, flex-direction, justify-content, align-items
// (including stretch), align-content, gap, flex-grow/flex-shrink/
// flex-basis, and flex-wrap (row direction only - column's main axis has
// no fixed budget to wrap against, same reasoning its justify-content/
// grow/shrink already don't apply there, see LayoutFlexLine; align-content
// is a row-only, wrap-only concept too, for the same reason - see
// MeasureLineCrossSize above and the align-content distribution below).
// No CSS Grid, no `order` (items lay out in DOM order). Reuses the same
// NullRenderer dry-run idiom TableWidgetHandler already established for
// its own grid columns/rows: measure every child's natural size first,
// then distribute/position/paint, one flex line at a time (LayoutFlexLine
// above). Lays children out into state.x/state.y/state.maxWidth (the
// caller - ContainerWidgetHandler::Render - has already applied this
// container's own margin/padding/width to those by the time it calls
// this), advancing state.y by however much cross-axis (row) or
// main-axis (column) extent the children consumed - the same contract
// the ordinary block-flow child loop it replaces has. align-content only
// ever has anything to redistribute when the container has an explicit
// CSS height (widget.hasHeight - already resolved box-model state by
// this point, same as the rest of Part 2) taller than what the wrapped
// lines naturally need; an auto-height container's cross size *is* its
// content's, with nothing left over - real CSS behaves the same way.
void RenderFlexContainer(const Widget &widget, LayoutState &state) {
  int n = widget.childCount;
  if (n == 0) {
    return;
  }
  bool isRow = widget.flexDirection == FlexDirection::kRow;
  bool wrapEnabled = isRow && widget.flexWrap == FlexWrap::kWrap;

  // Pass 1: each child's own natural width, in isolation - capped to
  // this container's available width (a flex item's text still wraps
  // against the container, same as any other child would) - and its
  // *render* width, which prioritizes the item's own explicit width
  // over that natural/content-driven one (see MeasureFlexItemNaturalWidth's
  // own doc comment for why a kContainer's naturalWidth deliberately
  // bypasses its own explicit width). renderWidth is what every later
  // computation here actually wants; naturalWidth only still matters as
  // one specific *fallback value* within it and within basis below.
  std::vector<float> naturalWidth(n);
  std::vector<float> renderWidth(n);
  for (int i = 0; i < n; ++i) {
    naturalWidth[i] =
        MeasureFlexItemNaturalWidth(widget.children[i], state.renderer, state.maxWidth);
    const Widget &child = widget.children[i];
    renderWidth[i] = (child.hasWidth && !child.widthIsPercent) ? child.width : naturalWidth[i];
  }

  // Each item's flex basis - its starting main size before grow/shrink
  // redistribute free space. Real CSS's flex-basis: auto falls back to
  // the width/height property for the main axis, if the item has one of
  // its own, else its natural content size.
  std::vector<float> basis(n);
  for (int i = 0; i < n; ++i) {
    const Widget &child = widget.children[i];
    if (child.hasFlexBasis) {
      basis[i] = child.flexBasis;
    } else if (isRow && child.hasWidth && !child.widthIsPercent) {
      basis[i] = child.width;
    } else if (!isRow && child.hasHeight) {
      basis[i] = child.height;
    } else if (isRow) {
      basis[i] = naturalWidth[i];
    } else {
      basis[i] = MeasureItemHeightAt(child, state.renderer, renderWidth[i]);
    }
  }

  // Greedily pack items into lines when wrapping - a new line starts
  // whenever the next item wouldn't fit within state.maxWidth (always
  // placing at least one item per line, even one that alone overflows,
  // same as real CSS).
  std::vector<std::pair<int, int>> lines;
  if (!wrapEnabled) {
    lines.emplace_back(0, n);
  } else {
    int lineStart = 0;
    float lineTotal = basis[0];
    for (int i = 1; i < n; ++i) {
      float withItem = lineTotal + widget.gap + basis[i];
      if (withItem > state.maxWidth) {
        lines.emplace_back(lineStart, i - lineStart);
        lineStart = i;
        lineTotal = basis[i];
      } else {
        lineTotal = withItem;
      }
    }
    lines.emplace_back(lineStart, n - lineStart);
  }

  // align-content: only meaningful for row (column never wraps into more
  // than one line - see this function's own doc comment) and only when
  // there's actual extra space to redistribute, i.e. an explicit CSS
  // height taller than what the lines naturally need. Pre-measuring
  // every line's natural cross size up front (rather than learning it
  // line-by-line, which is what the loop below used to do as its only
  // job) is what makes that "taller than natural" comparison possible
  // before any line's final position is committed to.
  std::vector<float> naturalCrossSize(lines.size(), 0.0f);
  float leadingCross = 0.0f;
  float betweenCrossGap = widget.gap;
  float perLineStretch = 0.0f;
  if (isRow) {
    float totalNaturalCross = static_cast<float>(lines.size() - 1) * widget.gap;
    for (size_t li = 0; li < lines.size(); ++li) {
      naturalCrossSize[li] = MeasureLineCrossSize(widget, state, isRow, lines[li].first,
                                                    lines[li].second, renderWidth);
      totalNaturalCross += naturalCrossSize[li];
    }

    float extraCrossSpace =
        widget.hasHeight ? std::max(0.0f, widget.height - totalNaturalCross) : 0.0f;
    if (extraCrossSpace > 0.0f) {
      switch (widget.alignContent) {
      case AlignContent::kFlexStart:
        break;
      case AlignContent::kCenter:
        leadingCross = extraCrossSpace / 2.0f;
        break;
      case AlignContent::kFlexEnd:
        leadingCross = extraCrossSpace;
        break;
      case AlignContent::kSpaceBetween:
        if (lines.size() > 1) {
          betweenCrossGap =
              widget.gap + extraCrossSpace / static_cast<float>(lines.size() - 1);
        }
        break;
      case AlignContent::kSpaceAround: {
        float perLineGap = extraCrossSpace / static_cast<float>(lines.size());
        leadingCross = perLineGap / 2.0f;
        betweenCrossGap = widget.gap + perLineGap;
        break;
      }
      case AlignContent::kStretch:
        perLineStretch = extraCrossSpace / static_cast<float>(lines.size());
        break;
      }
    }
  }

  // Row: accumulates each line's own cross size (+ betweenCrossGap
  // between lines - align-content:space-between/space-around's own
  // enlarged gap, or just widget.gap otherwise) into the total
  // cross-axis extent every line together consumed. Column: never more
  // than one line, so this is just that single line's own main-axis
  // extent.
  float stateYAdvance = 0.0f;
  float crossCursor = leadingCross;
  for (size_t li = 0; li < lines.size(); ++li) {
    int startIndex = lines[li].first;
    int count = lines[li].second;

    float mainAxisAvailable = state.maxWidth;
    if (!isRow) {
      mainAxisAvailable = static_cast<float>(count - 1) * widget.gap;
      for (int i = 0; i < count; ++i) {
        mainAxisAvailable += basis[startIndex + i];
      }
    }

    float minCrossSize = isRow ? naturalCrossSize[li] + perLineStretch : 0.0f;
    FlexLineResult result = LayoutFlexLine(widget, state, isRow, startIndex, count,
                                            renderWidth, basis, mainAxisAvailable,
                                            crossCursor, minCrossSize);

    if (isRow) {
      crossCursor += result.crossSize;
      if (li + 1 < lines.size()) {
        crossCursor += betweenCrossGap;
      }
      stateYAdvance = crossCursor;
    } else {
      stateYAdvance = result.mainExtent;
    }
  }

  state.y += stateYAdvance;
}

// Like MeasureHeightAt, but for a flex container's own background/
// border box: MeasureHeightAt just loops children in plain block flow,
// oblivious to display:flex (gap, row-vs-column, everything
// RenderFlexContainer actually does) - a flex container's own box needs
// its height measured the same way it actually gets laid out, not as if
// it were an ordinary block-flow parent.
float MeasureFlexHeightAt(const Widget &widget, const IRenderer &renderer,
                           float width) {
  NullRenderer nullRenderer(renderer);
  LayoutState dryRun{nullRenderer, 0.0f, 0.0f, width};
  RenderFlexContainer(widget, dryRun);
  FlushLine(dryRun);
  return dryRun.y;
}

// One WidgetHandler subclass per WidgetKind. The Widget tree itself stays
// a plain tagged struct (so it can live as static const data with zero
// runtime construction) - only the *behavior* per kind is isolated here.
class WidgetHandler {
public:
  virtual ~WidgetHandler() = default;

  virtual void Render(const Widget &widget, LayoutState &state) const = 0;
};

class TextWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    Color color = widget.hasColor ? widget.color : kDefaultTextColor;
    AppendWrappedText(state, widget.text, widget.fontSize, widget.bold, color);
  }
};

class LineBreakWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    if (!state.pendingLine.empty()) {
      FlushLine(state);
    } else {
      state.y += state.renderer.LineHeight(widget.fontSize, widget.bold);
    }
  }
};

class RuleWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    bool suppressSpacing = ConsumeSuppressBlockSpacing(state);
    FlushLine(state);
    if (!suppressSpacing) {
      state.y += kBlockSpacing;
    }
    Color color = widget.hasBorderColor ? widget.borderColor : kDefaultBorderColor;
    state.renderer.DrawRect(state.x, state.y, state.maxWidth, kRuleHeight,
                             widget.borderWidth, color);
    state.y += kRuleHeight;
    if (!suppressSpacing) {
      state.y += kBlockSpacing;
    }
  }
};

// Draws a kCheckbox/kRadio's fixed-size indicator (square or circle,
// filled when checked) at (state.x, state.y), registers its BoxRegion the
// same way a text kBox does, and advances state.y - the same
// flush-before/register-a-hit-rect/advance-y shape BoxWidgetHandler
// itself follows for kText, just with no label/border/caret to draw.
void RenderCheckable(const Widget &widget, LayoutState &state, bool suppressSpacing) {
  FlushLine(state);
  if (!suppressSpacing) {
    state.y += kBlockSpacing;
  }

  // Margin only - a checkbox/radio's indicator is a fixed-size native
  // control (kCheckableSize), so width/height/padding don't apply the
  // way they do for a text kBox, but margin still spaces it from
  // whatever comes before/after it same as any other box-model kind.
  const float boxX = state.x + widget.marginLeft;
  state.y += widget.marginTop;

  Color borderColor = widget.hasBorderColor ? widget.borderColor : kDefaultBorderColor;
  const float cx = boxX + kCheckableSize / 2.0f;
  const float cy = state.y + kCheckableSize / 2.0f;

  if (widget.boxKind == BoxKind::kRadio) {
    state.renderer.DrawCircle(cx, cy, kCheckableSize / 2.0f,
                               widget.borderWidth, borderColor);
    if (widget.checked) {
      Color fillColor = widget.hasColor ? widget.color : kDefaultTextColor;
      state.renderer.DrawFilledCircle(
          cx, cy, kCheckableSize / 2.0f - kCheckableInset, fillColor);
    }
  } else {
    state.renderer.DrawRect(boxX, state.y, kCheckableSize, kCheckableSize,
                             widget.borderWidth, borderColor);
    if (widget.checked) {
      Color fillColor = widget.hasColor ? widget.color : kDefaultTextColor;
      state.renderer.DrawFilledRect(
          boxX + kCheckableInset, state.y + kCheckableInset,
          kCheckableSize - 2.0f * kCheckableInset,
          kCheckableSize - 2.0f * kCheckableInset, fillColor);
    }
  }

  if (state.boxRegions != nullptr) {
    state.boxRegions->push_back(
        {widget.userData, boxX, state.y, kCheckableSize, kCheckableSize});
  }

  state.y += kCheckableSize;
  state.y += widget.marginBottom;
  if (!suppressSpacing) {
    state.y += kBlockSpacing;
  }
}

class BoxWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    bool suppressSpacing = ConsumeSuppressBlockSpacing(state);
    if (widget.boxKind != BoxKind::kText) {
      RenderCheckable(widget, state, suppressSpacing);
      return;
    }

    FlushLine(state);

    // Margin: outside the border box, same semantics as
    // ContainerWidgetHandler's - unlike that handler this kind's box
    // never fills available width by default (a text input/button
    // shrinks to fit its label, not its container), so boxMaxWidth here
    // is only ever consulted below when an explicit width needs clamping
    // to what margin actually left available.
    const float boxX = state.x + widget.marginLeft;
    const float boxMaxWidth =
        std::max(0.0f, state.maxWidth - widget.marginLeft - widget.marginRight);
    state.y += widget.marginTop;

    const std::string label = widget.text != nullptr ? widget.text : "";
    const float lineHeight = state.renderer.LineHeight(widget.fontSize, widget.bold);
    const float textWidth =
        state.renderer.MeasureText(label, widget.fontSize, widget.bold);

    // This kind's own intrinsic default (kBoxPadding, same as ever) plus
    // whatever CSS padding adds on top - unlike kContainer, "unset
    // resolves to 0" here means "no *extra* padding beyond the built-in
    // default", not "no padding at all" (a text box with zero inset
    // around its label wouldn't be usable).
    const float padTop = kBoxPadding + widget.paddingTop;
    const float padRight = kBoxPadding + widget.paddingRight;
    const float padBottom = kBoxPadding + widget.paddingBottom;
    const float padLeft = kBoxPadding + widget.paddingLeft;

    float width = std::max(kBoxMinWidth, textWidth + padLeft + padRight);
    float height = lineHeight + padTop + padBottom;

    // Explicit width overrides the content-driven size outright (real
    // CSS: an author width always wins over shrink-to-fit), clamped to
    // what margin left available - same clamp formula
    // ContainerWidgetHandler uses, just against this kind's own
    // (content-driven, not fill-available) natural width instead of a
    // starting boxMaxWidth. An explicit height only ever grows the box:
    // there's no wrapping to reclaim space from, so shrinking below the
    // label's own natural height would just clip it.
    if (widget.hasWidth) {
      float resolvedWidth = widget.widthIsPercent
                                 ? state.maxWidth * widget.width / 100.0f
                                 : widget.width;
      width = std::min(boxMaxWidth, std::max(0.0f, resolvedWidth));
    }
    if (widget.hasHeight) {
      height = std::max(height, widget.height);
    }

    if (!suppressSpacing) {
      state.y += kBlockSpacing;
    }

    const float textX = boxX + padLeft;
    const float textY = state.y + padTop;
    const bool hasSelection = widget.cursorPos >= 0 &&
                               widget.selectionAnchor >= 0 &&
                               widget.selectionAnchor != widget.cursorPos;

    if (widget.hasBackgroundColor) {
      state.renderer.DrawFilledRect(boxX, state.y, width, height,
                                     widget.backgroundColor);
    }

    if (hasSelection) {
      size_t selStart = static_cast<size_t>(
          std::min(widget.cursorPos, widget.selectionAnchor));
      size_t selEnd = static_cast<size_t>(
          std::max(widget.cursorPos, widget.selectionAnchor));
      float startX =
          textX + state.renderer.MeasureText(label.substr(0, selStart),
                                              widget.fontSize, widget.bold);
      float endX =
          textX + state.renderer.MeasureText(label.substr(0, selEnd),
                                              widget.fontSize, widget.bold);
      state.renderer.DrawFilledRect(startX, textY, endX - startX, lineHeight,
                                     kSelectionColor);
    }

    Color borderColor = widget.hasBorderColor ? widget.borderColor : kDefaultBorderColor;
    state.renderer.DrawRect(boxX, state.y, width, height,
                             widget.borderWidth, borderColor);

    if (!label.empty()) {
      Color textColor = widget.hasColor ? widget.color : kDefaultTextColor;
      state.renderer.DrawText(label, textX, textY + widget.fontSize,
                               widget.fontSize, widget.bold, textColor);
    }

    if (widget.cursorPos >= 0 && !hasSelection) {
      float caretX =
          textX + state.renderer.MeasureText(
                      label.substr(0, static_cast<size_t>(widget.cursorPos)),
                      widget.fontSize, widget.bold);
      state.renderer.DrawFilledRect(caretX, textY, kCaretWidth, lineHeight,
                                     kSelectionColor);
    }

    if (state.boxRegions != nullptr) {
      state.boxRegions->push_back(
          {widget.userData, boxX, state.y, width, height, padLeft});
    }

    state.y += height;
    state.y += widget.marginBottom;
    if (!suppressSpacing) {
      state.y += kBlockSpacing;
    }
  }
};

// Shared by LinkWidgetHandler/LabelWidgetHandler below: single-line
// clickable text with the same margin/padding/explicit-width-height/
// background/border box model kBox's text handler has - minus that
// kind's own intrinsic default padding, since a link/label isn't a boxy
// control with a built-in inset of its own; unset padding here really is
// zero, same as kContainer. `underline` is link-only (see
// kLinkUnderlineHeight).
void RenderTextLikeBox(const Widget &widget, LayoutState &state,
                        bool suppressSpacing, bool underline) {
  FlushLine(state);

  const float boxX = state.x + widget.marginLeft;
  const float boxMaxWidth =
      std::max(0.0f, state.maxWidth - widget.marginLeft - widget.marginRight);
  state.y += widget.marginTop;

  const std::string label = widget.text != nullptr ? widget.text : "";
  const float lineHeight = state.renderer.LineHeight(widget.fontSize, widget.bold);
  const float textWidth =
      state.renderer.MeasureText(label, widget.fontSize, widget.bold);

  float width = textWidth + widget.paddingLeft + widget.paddingRight;
  float height = lineHeight + widget.paddingTop + widget.paddingBottom;

  if (widget.hasWidth) {
    float resolvedWidth = widget.widthIsPercent
                               ? state.maxWidth * widget.width / 100.0f
                               : widget.width;
    width = std::min(boxMaxWidth, std::max(0.0f, resolvedWidth));
  }
  if (widget.hasHeight) {
    height = std::max(height, widget.height);
  }

  if (!suppressSpacing) {
    state.y += kBlockSpacing;
  }

  if (widget.hasBackgroundColor) {
    state.renderer.DrawFilledRect(boxX, state.y, width, height,
                                   widget.backgroundColor);
  }
  if (widget.hasBorderColor) {
    state.renderer.DrawRect(boxX, state.y, width, height, widget.borderWidth,
                             widget.borderColor);
  }

  const float textX = boxX + widget.paddingLeft;
  const float textY = state.y + widget.paddingTop;

  if (!label.empty()) {
    Color color = widget.hasColor ? widget.color : kDefaultTextColor;
    state.renderer.DrawText(label, textX, textY + widget.fontSize,
                             widget.fontSize, widget.bold, color);
    if (underline) {
      state.renderer.DrawFilledRect(textX, textY + lineHeight - kLinkUnderlineHeight,
                                     textWidth, kLinkUnderlineHeight, color);
    }
  }

  if (state.boxRegions != nullptr) {
    state.boxRegions->push_back({widget.userData, boxX, state.y, width, height});
  }

  state.y += height;
  state.y += widget.marginBottom;
  if (!suppressSpacing) {
    state.y += kBlockSpacing;
  }
}

// <a>: RenderTextLikeBox with an underline.
class LinkWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    bool suppressSpacing = ConsumeSuppressBlockSpacing(state);
    RenderTextLikeBox(widget, state, suppressSpacing, /*underline=*/true);
  }
};

// <label for="...">: RenderTextLikeBox, no underline - a label isn't a
// navigation affordance the way a link is, so nothing marks it as such
// visually. main.cpp resolves userData's `for` attribute and activates
// whatever it points at (focus a text input, toggle a checkbox/radio,
// click a button).
class LabelWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    bool suppressSpacing = ConsumeSuppressBlockSpacing(state);
    RenderTextLikeBox(widget, state, suppressSpacing, /*underline=*/false);
  }
};

class ImageWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    bool suppressSpacing = ConsumeSuppressBlockSpacing(state);
    FlushLine(state);

    // A percentage width can only be resolved here, against the current
    // layout width - artisanc can't know the viewport size at build time.
    float explicitWidth = widget.imageWidthIsPercent
                               ? state.maxWidth * widget.imageWidth / 100.0f
                               : widget.imageWidth;

    if (!suppressSpacing) {
      state.y += kBlockSpacing;
    }
    float height = state.renderer.DrawImage(
        widget.imageData, widget.imageDataSize, state.x, state.y,
        state.maxWidth, explicitWidth, widget.imageHeight);
    state.y += height;
    if (!suppressSpacing) {
      state.y += kBlockSpacing;
    }
  }
};

// A cell anchored at its top-left grid position by GridPlacement below.
// `row`/`col` name the top-left cell of its span, not every cell it covers.
struct PlacedCell {
  const Widget *cell;
  int col;
};

// Assigns every <td>/<th> a top-left (row, col) grid position, accounting
// for colspan/rowspan: a rowspan carries a column's occupancy down into
// later rows, so a cell starting in row N may land in a column past where
// its DOM position alone would suggest. Mirrors how browsers place cells,
// simplified (spans are never shrunk to fit).
class GridPlacement {
public:
  explicit GridPlacement(const Widget &table)
      : rowCount_(table.childCount), occupied_(table.childCount),
        placements_(table.childCount) {
    for (int r = 0; r < rowCount_; ++r) {
      const Widget &row = table.children[r];
      int col = 0;

      for (int c = 0; c < row.childCount; ++c) {
        const Widget &cell = row.children[c];

        EnsureColumns(col + 1);
        while (IsOccupied(r, col)) {
          ++col;
          EnsureColumns(col + 1);
        }

        int colSpan = std::max(1, cell.colSpan);
        int rowSpan = std::max(1, cell.rowSpan);
        EnsureColumns(col + colSpan);
        Occupy(r, col, colSpan, rowSpan);

        placements_[r].push_back({&cell, col});
        col += colSpan;
      }
    }
  }

  int columnCount() const { return columnCount_; }
  const std::vector<PlacedCell> &row(int r) const { return placements_[r]; }

private:
  bool IsOccupied(int r, int c) const {
    return c < static_cast<int>(occupied_[r].size()) && occupied_[r][c];
  }

  void EnsureColumns(int needed) {
    if (needed <= columnCount_) {
      return;
    }
    columnCount_ = needed;
    for (auto &row : occupied_) {
      row.resize(columnCount_, false);
    }
  }

  void Occupy(int startRow, int col, int colSpan, int rowSpan) {
    int endRow = std::min(startRow + rowSpan, rowCount_);
    for (int r = startRow; r < endRow; ++r) {
      if (static_cast<int>(occupied_[r].size()) < col + colSpan) {
        occupied_[r].resize(col + colSpan, false);
      }
      for (int c = col; c < col + colSpan; ++c) {
        occupied_[r][c] = true;
      }
    }
  }

  int rowCount_;
  int columnCount_ = 0;
  std::vector<std::vector<bool>> occupied_;
  std::vector<std::vector<PlacedCell>> placements_;
};

// <table>: children are rows (plain kContainer widgets whose own children
// are kTableCell widgets, each an arbitrary nested Widget subtree) - laid
// out as a real grid, not flowed like other containers. Row/cell widgets
// are read directly here rather than through RenderWidget/HandlerFor,
// since positioning them is table-specific.
class TableWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &table, LayoutState &state) const override {
    bool suppressSpacing = ConsumeSuppressBlockSpacing(state);
    FlushLine(state);

    if (table.text != nullptr && table.text[0] != '\0') {
      Color captionColor = table.hasColor ? table.color : kDefaultTextColor;
      AppendWrappedText(state, table.text, table.fontSize, table.bold,
                         captionColor);
      FlushLine(state);
    }

    if (table.childCount == 0) {
      return;
    }

    GridPlacement grid(table);
    int columnCount = grid.columnCount();
    if (columnCount == 0) {
      return;
    }

    // Column width = widest single-column cell's natural (unwrapped, up to
    // the page width) content width, plus padding, across every row.
    std::vector<float> columnWidths(columnCount, kCellMinWidth);
    for (int r = 0; r < table.childCount; ++r) {
      for (const PlacedCell &placed : grid.row(r)) {
        if (std::max(1, placed.cell->colSpan) != 1) {
          continue;
        }
        float width = MeasureNaturalWidth(*placed.cell, state.renderer,
                                           state.maxWidth) +
                       2.0f * kCellPadding;
        columnWidths[placed.col] = std::max(columnWidths[placed.col], width);
      }
    }

    // A spanning cell may need more room than the columns it crosses
    // already have - spread the shortfall evenly across them.
    for (int r = 0; r < table.childCount; ++r) {
      for (const PlacedCell &placed : grid.row(r)) {
        int colSpan = std::max(1, placed.cell->colSpan);
        if (colSpan <= 1) {
          continue;
        }
        float needed = MeasureNaturalWidth(*placed.cell, state.renderer,
                                            state.maxWidth) +
                        2.0f * kCellPadding;

        float available = 0.0f;
        for (int c = placed.col; c < placed.col + colSpan; ++c) {
          available += columnWidths[c];
        }

        if (needed > available) {
          float extra = (needed - available) / static_cast<float>(colSpan);
          for (int c = placed.col; c < placed.col + colSpan; ++c) {
            columnWidths[c] += extra;
          }
        }
      }
    }

    // Natural width is measured per cell against the full page width, with
    // no idea how many other columns need to share that space - so the
    // total can easily exceed what's actually available. Scale every
    // column down proportionally to fit rather than overflowing the page;
    // MeasureHeightAt below then wraps each cell to its real, final width.
    float totalWidth = 0.0f;
    for (float width : columnWidths) {
      totalWidth += width;
    }
    if (totalWidth > state.maxWidth) {
      float scale = state.maxWidth / totalWidth;
      for (float &width : columnWidths) {
        width *= scale;
      }
    }

    std::vector<float> columnX(columnCount);
    float x = 0.0f;
    for (int c = 0; c < columnCount; ++c) {
      columnX[c] = x;
      x += columnWidths[c];
    }

    // Row height = the tallest content among cells that *start* in that
    // row, once wrapped to the width their column(s) actually got. A
    // rowspan cell is excluded here (see below) - it's sized from summing
    // the rows it crosses instead, so only unusually tall rowspan content
    // can still overflow past them, which this simplified model accepts.
    std::vector<float> rowHeights(table.childCount, kCellMinHeight);
    for (int r = 0; r < table.childCount; ++r) {
      for (const PlacedCell &placed : grid.row(r)) {
        if (std::max(1, placed.cell->rowSpan) != 1) {
          continue;
        }

        int colSpan = std::max(1, placed.cell->colSpan);
        float cellWidth = 0.0f;
        for (int c = placed.col; c < placed.col + colSpan; ++c) {
          cellWidth += columnWidths[c];
        }

        float height = MeasureHeightAt(*placed.cell, state.renderer,
                                        std::max(0.0f, cellWidth -
                                                            2.0f * kCellPadding)) +
                        2.0f * kCellPadding;
        rowHeights[r] = std::max(rowHeights[r], height);
      }
    }

    std::vector<float> rowY(table.childCount);
    float y = 0.0f;
    for (int r = 0; r < table.childCount; ++r) {
      rowY[r] = y;
      y += rowHeights[r];
    }
    float tableHeight = y;

    if (!suppressSpacing) {
      state.y += kBlockSpacing;
    }

    for (int r = 0; r < table.childCount; ++r) {
      for (const PlacedCell &placed : grid.row(r)) {
        int colSpan = std::max(1, placed.cell->colSpan);
        int rowSpan = std::max(1, placed.cell->rowSpan);
        int endRow = std::min(r + rowSpan, table.childCount);

        float cellWidth = 0.0f;
        for (int c = placed.col; c < placed.col + colSpan; ++c) {
          cellWidth += columnWidths[c];
        }
        float cellHeight = rowY[endRow - 1] + rowHeights[endRow - 1] - rowY[r];

        float cellX = state.x + columnX[placed.col];
        float cellY = state.y + rowY[r];

        if (placed.cell->hasBackgroundColor) {
          state.renderer.DrawFilledRect(cellX, cellY, cellWidth, cellHeight,
                                         placed.cell->backgroundColor);
        }
        Color cellBorderColor = placed.cell->hasBorderColor
                                     ? placed.cell->borderColor
                                     : kDefaultBorderColor;
        state.renderer.DrawRect(cellX, cellY, cellWidth, cellHeight,
                                 placed.cell->borderWidth, cellBorderColor);
        RenderContentAt(*placed.cell, state.renderer, cellX + kCellPadding,
                         cellY + kCellPadding,
                         std::max(0.0f, cellWidth - 2.0f * kCellPadding));
      }
    }

    state.y += tableHeight;
    if (!suppressSpacing) {
      state.y += kBlockSpacing;
    }
  }
};

class ContainerWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    bool suppressSpacing = ConsumeSuppressBlockSpacing(state);

    // Only block-level containers break inline flow; inline ones (span,
    // a, ...) let their children keep sharing the parent's current line
    // - and never apply box-model properties either: they have no width
    // of their own to inset/size against in this flow model, same
    // reasoning the background/border-box painting below already used
    // before box-model properties existed.
    if (!widget.blockSpacing) {
      for (int i = 0; i < widget.childCount; ++i) {
        RenderWidget(widget.children[i], state);
      }
      return;
    }

    FlushLine(state);
    if (!suppressSpacing) {
      state.y += kBlockSpacing;
    }

    // Margin: outside the border box, insets from whatever space this
    // container inherited from its own parent - resolved first, before
    // background/border is ever painted.
    float boxX = state.x + widget.marginLeft;
    float boxMaxWidth =
        std::max(0.0f, state.maxWidth - widget.marginLeft - widget.marginRight);
    state.y += widget.marginTop;
    // Border-box top - see the region push after this container's
    // content/padding are done below, which pairs with this to record
    // the same border box a background/border would paint into (see
    // that block a few lines down), so a hover hit-test lands on exactly
    // what a background/border visually would.
    float boxTopY = state.y;

    // Explicit CSS width clamps/overrides the natural content width. A
    // percentage resolves against state.maxWidth (the space this
    // container's *parent* handed it, before this container's own
    // margin - real CSS's containing-block semantics: this element's
    // own margin doesn't change what "100%" means for its width), but
    // the final box never gets to exceed what margin actually left
    // available - this flow model has no scroll-within-content to fall
    // back on the way real CSS's overflow would.
    if (widget.hasWidth) {
      float resolvedWidth = widget.widthIsPercent
                                 ? state.maxWidth * widget.width / 100.0f
                                 : widget.width;
      boxMaxWidth = std::min(boxMaxWidth, std::max(0.0f, resolvedWidth));
    }

    bool isFlex = widget.hasDisplay && widget.display == DisplayMode::kFlex;

    // A background/border wraps the container's own (margin-adjusted,
    // width-clamped) border box.
    if (widget.hasBackgroundColor || widget.hasBorderColor) {
      float boxHeight;
      if (widget.hasHeight) {
        boxHeight = widget.height;
      } else if (isFlex) {
        boxHeight = MeasureFlexHeightAt(widget, state.renderer, boxMaxWidth);
      } else {
        boxHeight = MeasureHeightAt(widget, state.renderer, boxMaxWidth);
      }
      if (widget.hasBackgroundColor) {
        state.renderer.DrawFilledRect(boxX, state.y, boxMaxWidth, boxHeight,
                                       widget.backgroundColor);
      }
      if (widget.hasBorderColor) {
        state.renderer.DrawRect(boxX, state.y, boxMaxWidth, boxHeight,
                                 widget.borderWidth, widget.borderColor);
      }
    }

    // Padding: inside the border box. Children get a further-inset x/
    // maxWidth for the duration of this loop only - restored right
    // after, so a widget later in the *parent's* own sibling loop isn't
    // left seeing this container's insets.
    float savedX = state.x;
    float savedMaxWidth = state.maxWidth;
    state.x = boxX + widget.paddingLeft;
    state.maxWidth =
        std::max(0.0f, boxMaxWidth - widget.paddingLeft - widget.paddingRight);
    state.y += widget.paddingTop;

    float contentTopY = state.y;
    if (isFlex) {
      RenderFlexContainer(widget, state);
    } else {
      for (int i = 0; i < widget.childCount; ++i) {
        RenderWidget(widget.children[i], state);
      }
    }
    FlushLine(state);

    // An explicit height that's taller than what the content actually
    // needed still reserves the full height (matching real CSS: content
    // shorter than an explicit height doesn't shrink the box) - but
    // never *shrinks* state.y back below where the content naturally
    // reached, matching real CSS's overflow: visible default (content
    // taller than an explicit height spills past it rather than being
    // clipped, so whatever comes next still starts after the content,
    // not after the too-small explicit height).
    if (widget.hasHeight) {
      state.y = std::max(state.y, contentTopY + widget.height);
    }

    state.y += widget.paddingBottom;

    // The same border box background/border painting above used (or
    // would have, had this container had either) - registered so a
    // hover hit-test (main.cpp) can find this element even when it's
    // otherwise non-interactive (a plain <div>/<li>/...), the same way
    // BoxWidgetHandler/LinkWidgetHandler/LabelWidgetHandler already
    // register theirs. MakeContainer (widget_tree_builder.cpp) is the
    // only place that ever builds a block-level (blockSpacing) kContainer
    // Widget, and it always sets userData - no null check needed here.
    if (state.boxRegions != nullptr) {
      state.boxRegions->push_back(
          {widget.userData, boxX, boxTopY, boxMaxWidth, state.y - boxTopY});
    }

    state.x = savedX;
    state.maxWidth = savedMaxWidth;

    state.y += widget.marginBottom;
    if (!suppressSpacing) {
      state.y += kBlockSpacing;
    }
  }
};

const WidgetHandler &HandlerFor(WidgetKind kind) {
  static const TextWidgetHandler kText;
  static const LineBreakWidgetHandler kLineBreak;
  static const RuleWidgetHandler kRule;
  static const BoxWidgetHandler kBox;
  static const ImageWidgetHandler kImage;
  static const TableWidgetHandler kTable;
  static const ContainerWidgetHandler kContainer;
  static const LinkWidgetHandler kLink;
  static const LabelWidgetHandler kLabel;

  switch (kind) {
  case WidgetKind::kText:
    return kText;
  case WidgetKind::kLineBreak:
    return kLineBreak;
  case WidgetKind::kRule:
    return kRule;
  case WidgetKind::kBox:
    return kBox;
  case WidgetKind::kImage:
    return kImage;
  case WidgetKind::kTable:
    return kTable;
  case WidgetKind::kLink:
    return kLink;
  case WidgetKind::kLabel:
    return kLabel;
  case WidgetKind::kTableCell:
    // Never reached in practice - kTableCell widgets only appear as row
    // children inside a table's own array, which TableWidgetHandler reads
    // directly (via MeasureNaturalWidth/MeasureHeightAt/RenderContentAt)
    // rather than dispatching through here. It holds children like a
    // container, not `text`, so fall back to that shape if one ever
    // slipped through.
    return kContainer;
  case WidgetKind::kContainer:
    return kContainer;
  }

  return kContainer;
}

void RenderWidget(const Widget &widget, LayoutState &state) {
  HandlerFor(widget.kind).Render(widget, state);
}

} // namespace

int CharIndexAtX(const IRenderer &renderer, const std::string &text,
                  float fontSize, float relativeX) {
  if (relativeX <= 0.0f || text.empty()) {
    return 0;
  }

  // Walk character-by-character, snapping to whichever side of each
  // character's midpoint relativeX falls on. O(n^2) in text length (each
  // step remeasures the whole prefix), which is fine for the short
  // strings a single-line input field holds. Always measured non-bold:
  // an <input>/<button>'s BoxRegion (the hit-test record this reads
  // against) doesn't carry the widget's resolved style, so a bold field's
  // caret could land very slightly off - not worth threading style
  // through BoxRegion just for this.
  float previousWidth = 0.0f;
  for (size_t i = 1; i <= text.size(); ++i) {
    float width = renderer.MeasureText(text.substr(0, i), fontSize, false);
    float midpoint = (previousWidth + width) / 2.0f;
    if (relativeX < midpoint) {
      return static_cast<int>(i - 1);
    }
    previousWidth = width;
  }

  return static_cast<int>(text.size());
}

WidgetRenderer::WidgetRenderer(IRenderer &renderer) : renderer_(renderer) {}

void WidgetRenderer::Render(const Widget &root, int viewportWidth,
                             std::vector<BoxRegion> *outBoxRegions) {
  if (outBoxRegions != nullptr) {
    outBoxRegions->clear();
  }

  LayoutState state{renderer_, kMargin, kMargin,
                     static_cast<float>(viewportWidth) - 2.0f * kMargin};
  state.boxRegions = outBoxRegions;

  RenderWidget(root, state);
  FlushLine(state);
}

float WidgetRenderer::MeasureContentHeight(const Widget &root,
                                            int viewportWidth) const {
  NullRenderer nullRenderer(renderer_);
  LayoutState state{nullRenderer, kMargin, kMargin,
                     static_cast<float>(viewportWidth) - 2.0f * kMargin};

  RenderWidget(root, state);
  FlushLine(state);

  return state.y + kMargin; // Symmetric bottom margin, matching the top.
}

} // namespace artisan
