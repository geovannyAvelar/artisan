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
};

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
    FlushLine(state);
    state.y += kBlockSpacing;
    Color color = widget.hasBorderColor ? widget.borderColor : kDefaultBorderColor;
    state.renderer.DrawRect(state.x, state.y, state.maxWidth, kRuleHeight,
                             widget.borderWidth, color);
    state.y += kRuleHeight + kBlockSpacing;
  }
};

// Draws a kCheckbox/kRadio's fixed-size indicator (square or circle,
// filled when checked) at (state.x, state.y), registers its BoxRegion the
// same way a text kBox does, and advances state.y - the same
// flush-before/register-a-hit-rect/advance-y shape BoxWidgetHandler
// itself follows for kText, just with no label/border/caret to draw.
void RenderCheckable(const Widget &widget, LayoutState &state) {
  FlushLine(state);
  state.y += kBlockSpacing;

  Color borderColor = widget.hasBorderColor ? widget.borderColor : kDefaultBorderColor;
  const float cx = state.x + kCheckableSize / 2.0f;
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
    state.renderer.DrawRect(state.x, state.y, kCheckableSize, kCheckableSize,
                             widget.borderWidth, borderColor);
    if (widget.checked) {
      Color fillColor = widget.hasColor ? widget.color : kDefaultTextColor;
      state.renderer.DrawFilledRect(
          state.x + kCheckableInset, state.y + kCheckableInset,
          kCheckableSize - 2.0f * kCheckableInset,
          kCheckableSize - 2.0f * kCheckableInset, fillColor);
    }
  }

  if (state.boxRegions != nullptr) {
    state.boxRegions->push_back(
        {widget.userData, state.x, state.y, kCheckableSize, kCheckableSize});
  }

  state.y += kCheckableSize + kBlockSpacing;
}

class BoxWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    if (widget.boxKind != BoxKind::kText) {
      RenderCheckable(widget, state);
      return;
    }

    FlushLine(state);

    const std::string label = widget.text != nullptr ? widget.text : "";
    const float lineHeight = state.renderer.LineHeight(widget.fontSize, widget.bold);
    const float textWidth =
        state.renderer.MeasureText(label, widget.fontSize, widget.bold);

    const float width = std::max(kBoxMinWidth, textWidth + 2.0f * kBoxPadding);
    const float height = lineHeight + 2.0f * kBoxPadding;

    state.y += kBlockSpacing;

    const float textX = state.x + kBoxPadding;
    const float textY = state.y + kBoxPadding;
    const bool hasSelection = widget.cursorPos >= 0 &&
                               widget.selectionAnchor >= 0 &&
                               widget.selectionAnchor != widget.cursorPos;

    if (widget.hasBackgroundColor) {
      state.renderer.DrawFilledRect(state.x, state.y, width, height,
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
    state.renderer.DrawRect(state.x, state.y, width, height,
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
          {widget.userData, state.x, state.y, width, height});
    }

    state.y += height + kBlockSpacing;
  }
};

// <a>: painted like BoxWidgetHandler's flush-before/register-a-hit-rect
// treatment, but with an underline instead of a border - a link isn't a
// boxy control, just clickable text. Single-line only (no word-wrap),
// same simplification BoxWidgetHandler already makes for button/input
// labels.
class LinkWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    FlushLine(state);

    const std::string label = widget.text != nullptr ? widget.text : "";
    const float lineHeight = state.renderer.LineHeight(widget.fontSize, widget.bold);
    const float textWidth =
        state.renderer.MeasureText(label, widget.fontSize, widget.bold);

    state.y += kBlockSpacing;

    if (!label.empty()) {
      Color color = widget.hasColor ? widget.color : kDefaultTextColor;
      state.renderer.DrawText(label, state.x, state.y + widget.fontSize,
                               widget.fontSize, widget.bold, color);
      state.renderer.DrawFilledRect(state.x, state.y + lineHeight - kLinkUnderlineHeight,
                                     textWidth, kLinkUnderlineHeight, color);
    }

    if (state.boxRegions != nullptr) {
      state.boxRegions->push_back(
          {widget.userData, state.x, state.y, textWidth, lineHeight});
    }

    state.y += lineHeight + kBlockSpacing;
  }
};

// <label for="...">: identical to LinkWidgetHandler, minus the underline -
// clickable text, but a label isn't a navigation affordance the way a
// link is, so nothing marks it as such visually. main.cpp resolves
// userData's `for` attribute and activates whatever it points at (focus a
// text input, toggle a checkbox/radio, click a button).
class LabelWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    FlushLine(state);

    const std::string label = widget.text != nullptr ? widget.text : "";
    const float lineHeight = state.renderer.LineHeight(widget.fontSize, widget.bold);
    const float textWidth =
        state.renderer.MeasureText(label, widget.fontSize, widget.bold);

    state.y += kBlockSpacing;

    if (!label.empty()) {
      Color color = widget.hasColor ? widget.color : kDefaultTextColor;
      state.renderer.DrawText(label, state.x, state.y + widget.fontSize,
                               widget.fontSize, widget.bold, color);
    }

    if (state.boxRegions != nullptr) {
      state.boxRegions->push_back(
          {widget.userData, state.x, state.y, textWidth, lineHeight});
    }

    state.y += lineHeight + kBlockSpacing;
  }
};

class ImageWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    FlushLine(state);

    // A percentage width can only be resolved here, against the current
    // layout width - artisanc can't know the viewport size at build time.
    float explicitWidth = widget.imageWidthIsPercent
                               ? state.maxWidth * widget.imageWidth / 100.0f
                               : widget.imageWidth;

    state.y += kBlockSpacing;
    float height = state.renderer.DrawImage(
        widget.imageData, widget.imageDataSize, state.x, state.y,
        state.maxWidth, explicitWidth, widget.imageHeight);
    state.y += height + kBlockSpacing;
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

    state.y += kBlockSpacing;

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

    state.y += tableHeight + kBlockSpacing;
  }
};

class ContainerWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    // Only block-level containers break inline flow; inline ones (span,
    // a, ...) let their children keep sharing the parent's current line.
    if (widget.blockSpacing) {
      FlushLine(state);
      state.y += kBlockSpacing;
    }

    // A background/border wraps the container's own content box - only
    // meaningful for block-level containers (div, p, ...): an inline one
    // (span) shares its parent's line and has no width of its own to
    // paint a tight background against in this flow model, so it's left
    // unpainted rather than incorrectly spanning the full page width.
    if (widget.blockSpacing &&
        (widget.hasBackgroundColor || widget.hasBorderColor)) {
      float contentHeight =
          MeasureHeightAt(widget, state.renderer, state.maxWidth);
      if (widget.hasBackgroundColor) {
        state.renderer.DrawFilledRect(state.x, state.y, state.maxWidth,
                                       contentHeight, widget.backgroundColor);
      }
      if (widget.hasBorderColor) {
        state.renderer.DrawRect(state.x, state.y, state.maxWidth,
                                 contentHeight, widget.borderWidth,
                                 widget.borderColor);
      }
    }

    for (int i = 0; i < widget.childCount; ++i) {
      RenderWidget(widget.children[i], state);
    }

    if (widget.blockSpacing) {
      FlushLine(state);
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
