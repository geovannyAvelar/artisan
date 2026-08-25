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
                           state.pendingFontSize);
  state.y += state.renderer.LineHeight(state.pendingFontSize);
  state.pendingLine.clear();
}

void AppendWrappedText(LayoutState &state, const std::string &text,
                        float fontSize) {
  // A font-size change (e.g. a heading right after inline text - shouldn't
  // normally happen since headings are block-level, but stay defensive)
  // can't share a line with what came before.
  if (!state.pendingLine.empty() && state.pendingFontSize != fontSize) {
    FlushLine(state);
  }
  state.pendingFontSize = fontSize;

  std::istringstream words(text);
  std::string word;

  while (words >> word) {
    std::string candidate =
        state.pendingLine.empty() ? word : state.pendingLine + " " + word;
    float width = state.renderer.MeasureText(candidate, fontSize);

    if (width > state.maxWidth && !state.pendingLine.empty()) {
      state.renderer.DrawText(state.pendingLine, state.x, state.y, fontSize);
      state.y += state.renderer.LineHeight(fontSize);
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
                float fontSize) override {
    naturalWidth_ = std::max(naturalWidth_, x + inner_.MeasureText(text, fontSize));
  }

  float MeasureText(const std::string &text, float fontSize) const override {
    return inner_.MeasureText(text, fontSize);
  }

  float LineHeight(float fontSize) const override {
    return inner_.LineHeight(fontSize);
  }

  void DrawRect(float x, float /*y*/, float width, float /*height*/) override {
    naturalWidth_ = std::max(naturalWidth_, x + width);
  }

  void DrawFilledRect(float x, float /*y*/, float width,
                       float /*height*/) override {
    naturalWidth_ = std::max(naturalWidth_, x + width);
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
    AppendWrappedText(state, widget.text, widget.fontSize);
  }
};

class LineBreakWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    if (!state.pendingLine.empty()) {
      FlushLine(state);
    } else {
      state.y += state.renderer.LineHeight(widget.fontSize);
    }
  }
};

class RuleWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget & /*widget*/, LayoutState &state) const override {
    FlushLine(state);
    state.y += kBlockSpacing;
    state.renderer.DrawRect(state.x, state.y, state.maxWidth, kRuleHeight);
    state.y += kRuleHeight + kBlockSpacing;
  }
};

class BoxWidgetHandler final : public WidgetHandler {
public:
  void Render(const Widget &widget, LayoutState &state) const override {
    FlushLine(state);

    const std::string label = widget.text != nullptr ? widget.text : "";
    const float lineHeight = state.renderer.LineHeight(widget.fontSize);
    const float textWidth =
        state.renderer.MeasureText(label, widget.fontSize);

    const float width = std::max(kBoxMinWidth, textWidth + 2.0f * kBoxPadding);
    const float height = lineHeight + 2.0f * kBoxPadding;

    state.y += kBlockSpacing;

    const float textX = state.x + kBoxPadding;
    const float textY = state.y + kBoxPadding;
    const bool hasSelection = widget.cursorPos >= 0 &&
                               widget.selectionAnchor >= 0 &&
                               widget.selectionAnchor != widget.cursorPos;

    if (hasSelection) {
      size_t selStart = static_cast<size_t>(
          std::min(widget.cursorPos, widget.selectionAnchor));
      size_t selEnd = static_cast<size_t>(
          std::max(widget.cursorPos, widget.selectionAnchor));
      float startX =
          textX + state.renderer.MeasureText(label.substr(0, selStart),
                                              widget.fontSize);
      float endX =
          textX + state.renderer.MeasureText(label.substr(0, selEnd),
                                              widget.fontSize);
      state.renderer.DrawFilledRect(startX, textY, endX - startX, lineHeight);
    }

    state.renderer.DrawRect(state.x, state.y, width, height);

    if (!label.empty()) {
      state.renderer.DrawText(label, textX, textY + widget.fontSize,
                               widget.fontSize);
    }

    if (widget.cursorPos >= 0 && !hasSelection) {
      float caretX =
          textX + state.renderer.MeasureText(
                      label.substr(0, static_cast<size_t>(widget.cursorPos)),
                      widget.fontSize);
      state.renderer.DrawFilledRect(caretX, textY, kCaretWidth, lineHeight);
    }

    if (state.boxRegions != nullptr) {
      state.boxRegions->push_back(
          {widget.userData, state.x, state.y, width, height});
    }

    state.y += height + kBlockSpacing;
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
      AppendWrappedText(state, table.text, table.fontSize);
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

        state.renderer.DrawRect(cellX, cellY, cellWidth, cellHeight);
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
  // strings a single-line input field holds.
  float previousWidth = 0.0f;
  for (size_t i = 1; i <= text.size(); ++i) {
    float width = renderer.MeasureText(text.substr(0, i), fontSize);
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
