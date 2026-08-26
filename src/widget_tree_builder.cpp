// Builds a Widget tree from a mutable Node tree - the one place a Node
// tree's structure gets interpreted as layout, regardless of whether that
// tree was compiled from markup by artisanc or built by hand/script at
// runtime. Tag classification here (block/inline, heading sizes, table
// grids, input/button/image handling) is the single source of truth for
// what an element "means" - artisanc itself no longer has any of this
// logic; it just mirrors DOM structure into Node-construction code.

#include "widget_tree_builder.h"

#include "css.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace artisan {

// An <input>'s `type` attribute picks checkbox/radio's small fixed-size
// indicator over the default single-line text box - anything else
// (including a missing `type`) keeps today's text behavior. Exposed (not
// anonymous-namespace-local like the rest of this file's tag-classification
// helpers) since main.cpp's click handling needs the same check to decide
// what a click on an <input> does - kept as one source of truth rather than
// two copies of the same string comparison.
bool IsCheckableInputType(const Node &node) {
  const std::string *type = node.GetAttribute("type");
  return type != nullptr && (*type == "checkbox" || *type == "radio");
}

namespace {

// Copies a resolved CSS declaration set onto the widget it produced -
// color/bold apply wherever WidgetRenderer draws text for this kind;
// background/border apply wherever it draws this kind's own rect (a
// kBox/kTable/kTableCell border, a <hr>, or a block-level kContainer's
// content box - see widget_renderer.cpp). borderWidth keeps Widget's own
// default (1px) when the stylesheet didn't set one, rather than being
// overwritten with Declarations' own default.
void ApplyStyle(Widget &widget, const Declarations &style) {
  widget.hasColor = style.hasColor;
  widget.color = style.color;
  widget.bold = style.bold;
  widget.hasBackgroundColor = style.hasBackgroundColor;
  widget.backgroundColor = style.backgroundColor;
  widget.hasBorderColor = style.hasBorderColor;
  widget.borderColor = style.borderColor;
  if (style.hasBorderWidth) {
    widget.borderWidth = style.borderWidth;
  }
  widget.hasWidth = style.hasWidth;
  widget.width = style.width;
  widget.widthIsPercent = style.widthIsPercent;
  widget.hasHeight = style.hasHeight;
  widget.height = style.height;
  // Declarations' own hasPaddingXxx/hasMarginXxx only matter for cascade
  // resolution (StyleSheet::Resolve/MergeInlineStyle) - by here, an
  // unset side has already resolved to 0, exactly what Widget's own
  // (flag-less) fields should hold, so a plain copy is correct.
  widget.paddingTop = style.paddingTop;
  widget.paddingRight = style.paddingRight;
  widget.paddingBottom = style.paddingBottom;
  widget.paddingLeft = style.paddingLeft;
  widget.marginTop = style.marginTop;
  widget.marginRight = style.marginRight;
  widget.marginBottom = style.marginBottom;
  widget.marginLeft = style.marginLeft;
  widget.hasDisplay = style.hasDisplay;
  widget.display = style.display;
  widget.flexDirection = style.flexDirection;
  widget.justifyContent = style.justifyContent;
  widget.alignItems = style.alignItems;
  widget.gap = style.gap;
}

bool IsAllWhitespace(const std::string &text) {
  return std::all_of(text.begin(), text.end(),
                      [](unsigned char c) { return std::isspace(c); });
}

std::string CollapseWhitespace(const std::string &text) {
  std::istringstream words(text);
  std::string word;
  std::string result;

  while (words >> word) {
    if (!result.empty()) {
      result += ' ';
    }
    result += word;
  }

  return result;
}

// A single line worth of a subtree's text - used for labels (<button>,
// <caption>) that flatten their content instead of laying it out.
std::string FlattenText(const Node &node) {
  return CollapseWhitespace(node.textContent());
}

bool IsSkippedTag(const std::string &tag) {
  return tag == "head" || tag == "script" || tag == "style" ||
         tag == "title" || tag == "meta" || tag == "link";
}


// Concatenates the text content of every <style> element anywhere in
// `node`'s subtree, depth-first - a document can have more than one, and
// (like a real browser) its rules apply document-wide regardless of
// where in the tree they physically sit, not just to elements after
// them. Note this only ever sees <style> inside <body>: HtmlDocument
// only exposes the body element (see html_document.cpp), so a <style> in
// <head> - the conventional place to put one - never reaches the Node
// tree at all and is silently dropped, same as every other <head> tag.
void CollectStyleText(const Node &node, std::string &out) {
  for (const auto &childPtr : node.children()) {
    const Node &child = *childPtr;
    if (child.type() == NodeType::kElement && child.tagName() == "style") {
      out += child.textContent();
      out += '\n';
    }
    CollectStyleText(child, out);
  }
}

bool IsBlockTag(const std::string &tag) {
  static const std::vector<std::string> kBlockTags = {
      "html",   "body",   "div",    "p",      "ul",     "ol",
      "li",     "header", "footer", "section", "article", "nav", "main"};
  return std::find(kBlockTags.begin(), kBlockTags.end(), tag) !=
         kBlockTags.end();
}

float FontSizeForTag(const std::string &tag) {
  if (tag == "h1") return 32.0f;
  if (tag == "h2") return 26.0f;
  if (tag == "h3") return 22.0f;
  if (tag == "h4") return 19.0f;
  if (tag == "h5") return 17.0f;
  if (tag == "h6") return 15.0f;
  return kDefaultFontSize;
}

std::vector<Widget> BuildChildrenInits(const Node &parent, WidgetTree &tree,
                                        float fontSize,
                                        const InputFocus &focus,
                                        const StyleSheet &sheet,
                                        const Declarations &inheritedStyle);

Widget MakeContainer(const Node &node, WidgetTree &tree, float fontSize,
                      bool blockSpacing, const InputFocus &focus,
                      const StyleSheet &sheet, const Declarations &style) {
  std::vector<Widget> childInits =
      BuildChildrenInits(node, tree, fontSize, focus, sheet, style);
  int count = static_cast<int>(childInits.size());
  const Widget *childrenPtr = tree.StoreArray(std::move(childInits));

  Widget widget{WidgetKind::kContainer,
                blockSpacing,
                0.0f,
                nullptr,
                childrenPtr,
                count,
                nullptr,
                0,
                0.0f,
                0.0f,
                false,
                1,
                1};
  ApplyStyle(widget, style);
  return widget;
}

Widget MakeBox(WidgetTree &tree, const std::string &label, float fontSize,
                const void *userData, const Declarations &style,
                int cursorPos = -1, int selectionAnchor = -1) {
  Widget widget{WidgetKind::kBox,
                false,
                fontSize,
                tree.StoreString(label),
                nullptr,
                0,
                nullptr,
                0,
                0.0f,
                0.0f,
                false,
                1,
                1};
  widget.userData = userData;
  widget.cursorPos = cursorPos;
  widget.selectionAnchor = selectionAnchor;
  ApplyStyle(widget, style);
  return widget;
}

// <a>: a single-line clickable label, same as MakeBox's FlattenText
// treatment for a <button> - the underlying text isn't word-wrapped or
// otherwise laid out as flowing inline content (see widget_renderer.cpp's
// LinkWidgetHandler for why).
Widget MakeLink(WidgetTree &tree, const Node &node, float fontSize,
                 const Declarations &style) {
  Widget widget{WidgetKind::kLink,
                false,
                fontSize,
                tree.StoreString(FlattenText(node)),
                nullptr,
                0,
                nullptr,
                0,
                0.0f,
                0.0f,
                false,
                1,
                1};
  widget.userData = &node;
  ApplyStyle(widget, style);
  return widget;
}

// <label for="...">: same flattened-single-line-clickable-text shape as
// MakeLink, just a different WidgetKind so WidgetRenderer paints it
// without an underline (see LabelWidgetHandler).
Widget MakeLabel(WidgetTree &tree, const Node &node, float fontSize,
                  const Declarations &style) {
  Widget widget{WidgetKind::kLabel,
                false,
                fontSize,
                tree.StoreString(FlattenText(node)),
                nullptr,
                0,
                nullptr,
                0,
                0.0f,
                0.0f,
                false,
                1,
                1};
  widget.userData = &node;
  ApplyStyle(widget, style);
  return widget;
}

// <input type="checkbox"|"radio">: a small fixed-size toggle indicator,
// not a text box - no label of its own (see BoxKind in widget.h). `boxKind`
// picks square vs. circle in WidgetRenderer; `checked` is read straight
// from the source Node's `checked` attribute (presence, regardless of
// value, means checked - matching HTML boolean-attribute convention).
Widget MakeCheckable(WidgetTree &tree, const void *userData,
                      const Declarations &style, BoxKind boxKind,
                      bool checked) {
  Widget widget{WidgetKind::kBox,
                false,
                0.0f,
                nullptr,
                nullptr,
                0,
                nullptr,
                0,
                0.0f,
                0.0f,
                false,
                1,
                1};
  widget.userData = userData;
  widget.boxKind = boxKind;
  widget.checked = checked;
  ApplyStyle(widget, style);
  return widget;
}

Widget MakeLineBreak(float fontSize, const Declarations &style) {
  Widget widget{
      WidgetKind::kLineBreak, false, fontSize, nullptr, nullptr, 0,
      nullptr,                0,     0.0f,     0.0f,    false,   1,
      1};
  ApplyStyle(widget, style);
  return widget;
}

Widget MakeRule(const Declarations &style) {
  Widget widget{WidgetKind::kRule, false, 0.0f, nullptr, nullptr, 0,
                nullptr,           0,     0.0f, 0.0f,    false,   1,
                1};
  ApplyStyle(widget, style);
  return widget;
}

Widget MakeText(WidgetTree &tree, const std::string &text, float fontSize,
                 const Declarations &style) {
  Widget widget{WidgetKind::kText,
                false,
                fontSize,
                tree.StoreString(text),
                nullptr,
                0,
                nullptr,
                0,
                0.0f,
                0.0f,
                false,
                1,
                1};
  ApplyStyle(widget, style);
  return widget;
}

// A `width`/`height` attribute value: either a plain pixel number or a
// percentage (e.g. "50%"). `value` is 0 with isPercent false for an
// absent or unparsable attribute - "unspecified".
struct Length {
  float value = 0.0f;
  bool isPercent = false;
};

Length ParseLength(const std::string &attr) {
  if (attr.empty()) {
    return {};
  }

  std::string value = attr;
  bool isPercent = false;
  if (value.back() == '%') {
    isPercent = true;
    value.pop_back();
  }

  try {
    return {std::stof(value), isPercent};
  } catch (const std::exception &) {
    return {};
  }
}

Widget MakeImage(const Node &node) {
  const std::string *widthAttr = node.GetAttribute("width");
  const std::string *heightAttr = node.GetAttribute("height");
  Length width = ParseLength(widthAttr != nullptr ? *widthAttr : std::string());
  Length height =
      ParseLength(heightAttr != nullptr ? *heightAttr : std::string());

  if (height.isPercent) {
    // No containing-block height to resolve a percentage against in this
    // layout model - same call artisanc used to make at compile time.
    height = {};
  }

  Widget widget{WidgetKind::kImage,
                false,
                0.0f,
                nullptr,
                nullptr,
                0,
                node.imageData(),
                node.imageDataSize(),
                width.value,
                height.value,
                width.isPercent,
                1,
                1};
  return widget;
}

// Parses a colspan/rowspan attribute value. A missing or invalid value
// (or anything less than 1) normalizes to 1, since 0 would break grid
// placement math downstream.
int ParseSpan(const Node &node, const char *attrName) {
  const std::string *attr = node.GetAttribute(attrName);
  if (attr == nullptr || attr->empty()) {
    return 1;
  }

  try {
    int value = std::stoi(*attr);
    return value >= 1 ? value : 1;
  } catch (const std::exception &) {
    return 1;
  }
}

// Collects every <tr> descendant of `node`, recursing through
// <thead>/<tbody>/<tfoot> wrappers (and ignoring anything else) - so both
// a bare <table><tr>...</table> and a full
// <table><thead>...</thead><tbody>...</tbody></table> work the same way.
void CollectTableRows(const Node &node, std::vector<const Node *> &rows) {
  for (const auto &childPtr : node.children()) {
    const Node &child = *childPtr;
    if (child.type() != NodeType::kElement) {
      continue;
    }

    const std::string &tag = child.tagName();
    if (tag == "tr") {
      rows.push_back(&child);
    } else if (tag == "thead" || tag == "tbody" || tag == "tfoot") {
      CollectTableRows(child, rows);
    }
  }
}

// Collects the <td>/<th> children of a single <tr>.
void CollectTableCells(const Node &rowNode, std::vector<const Node *> &cells) {
  for (const auto &childPtr : rowNode.children()) {
    const Node &child = *childPtr;
    if (child.type() != NodeType::kElement) {
      continue;
    }

    const std::string &tag = child.tagName();
    if (tag == "td" || tag == "th") {
      cells.push_back(&child);
    }
  }
}

// A <table>'s direct-child <caption>, if any (HTML allows at most one,
// and it isn't a row/cell so CollectTableRows never sees it).
std::string FindCaptionText(const Node &tableNode) {
  for (const auto &childPtr : tableNode.children()) {
    const Node &child = *childPtr;
    if (child.type() == NodeType::kElement && child.tagName() == "caption") {
      return FlattenText(child);
    }
  }
  return std::string();
}

// A cell's content is built exactly like any container's children (full
// nested markup - paragraphs, divs, images, even another table), not
// flattened to text: TableWidgetHandler runs the same block-flow layout
// on it to size and paint it within whatever width its column gets.
Widget MakeTableCell(const Node &node, WidgetTree &tree, float fontSize,
                      const InputFocus &focus, const StyleSheet &sheet,
                      const Declarations &style) {
  std::vector<Widget> childInits =
      BuildChildrenInits(node, tree, fontSize, focus, sheet, style);
  int count = static_cast<int>(childInits.size());
  const Widget *childrenPtr = tree.StoreArray(std::move(childInits));

  int colSpan = ParseSpan(node, "colspan");
  int rowSpan = ParseSpan(node, "rowspan");

  Widget widget{WidgetKind::kTableCell,
                false,
                0.0f,
                nullptr,
                childrenPtr,
                count,
                nullptr,
                0,
                0.0f,
                0.0f,
                false,
                colSpan,
                rowSpan};
  ApplyStyle(widget, style);
  return widget;
}

// <table> - flattens away thead/tbody/tfoot wrappers into a plain list of
// rows. Each row is an ordinary kContainer whose children are kTableCell
// widgets holding the cell's full nested content. TableWidgetHandler lays
// the whole thing out as a real grid at render time, honoring each cell's
// colspan/rowspan and painting the <caption> above it.
Widget MakeTable(const Node &node, WidgetTree &tree, float fontSize,
                  const InputFocus &focus, const StyleSheet &sheet,
                  const Declarations &style) {
  std::string captionText = FindCaptionText(node);

  std::vector<const Node *> rowNodes;
  CollectTableRows(node, rowNodes);

  std::vector<Widget> rowInits;
  for (const Node *rowNode : rowNodes) {
    // Cascades table -> row -> cell -> cell's own children, same as any
    // other nesting - a <tr class="highlight"> can color its cells' text
    // without each <td> needing its own class.
    Declarations rowStyle = sheet.Resolve(*rowNode, style);

    std::vector<const Node *> cellNodes;
    CollectTableCells(*rowNode, cellNodes);

    std::vector<Widget> cellInits;
    for (const Node *cellNode : cellNodes) {
      Declarations cellStyle = sheet.Resolve(*cellNode, rowStyle);
      cellInits.push_back(
          MakeTableCell(*cellNode, tree, fontSize, focus, sheet, cellStyle));
    }

    int cellCount = static_cast<int>(cellInits.size());
    const Widget *cellsPtr = tree.StoreArray(std::move(cellInits));

    Widget rowWidget{WidgetKind::kContainer,
                      false,
                      0.0f,
                      nullptr,
                      cellsPtr,
                      cellCount,
                      nullptr,
                      0,
                      0.0f,
                      0.0f,
                      false,
                      1,
                      1};
    ApplyStyle(rowWidget, rowStyle);
    rowInits.push_back(rowWidget);
  }

  int rowCount = static_cast<int>(rowInits.size());
  const Widget *rowsPtr = tree.StoreArray(std::move(rowInits));

  Widget widget{WidgetKind::kTable,
                false,
                fontSize,
                captionText.empty() ? nullptr : tree.StoreString(captionText),
                rowsPtr,
                rowCount,
                nullptr,
                0,
                0.0f,
                0.0f,
                false,
                1,
                1};
  ApplyStyle(widget, style);
  return widget;
}

std::string InputLabel(const Node &node) {
  const std::string *value = node.GetAttribute("value");
  if (value != nullptr && !value->empty()) {
    return *value;
  }
  const std::string *placeholder = node.GetAttribute("placeholder");
  return placeholder != nullptr ? *placeholder : std::string();
}

std::vector<Widget> BuildChildrenInits(const Node &parent, WidgetTree &tree,
                                        float fontSize,
                                        const InputFocus &focus,
                                        const StyleSheet &sheet,
                                        const Declarations &inheritedStyle) {
  std::vector<Widget> inits;

  for (const auto &childPtr : parent.children()) {
    const Node &child = *childPtr;

    if (child.type() == NodeType::kText) {
      const std::string &text = child.textContent();
      if (IsAllWhitespace(text)) {
        continue;
      }
      inits.push_back(MakeText(tree, text, fontSize, inheritedStyle));
      continue;
    }

    const std::string &tag = child.tagName();
    if (IsSkippedTag(tag)) {
      continue;
    }

    // Every element resolves its own style once, against whatever its
    // parent already resolved (inheritedStyle) - color/font-weight
    // cascade down from there; background/border never do (see
    // Declarations, css.h). Text nodes above skip this entirely and just
    // take inheritedStyle directly, since they have no tag/class/id of
    // their own to match a selector against.
    Declarations style = sheet.Resolve(child, inheritedStyle);
    // An inline style="..." attribute wins over the stylesheet cascade
    // regardless of what matched above - same precedence real CSS gives
    // inline style, above even an #id selector.
    MergeInlineStyle(child, style);

    if (tag == "br") {
      inits.push_back(MakeLineBreak(fontSize, style));
    } else if (tag == "hr") {
      inits.push_back(MakeRule(style));
    } else if (tag == "img") {
      inits.push_back(MakeImage(child));
    } else if (tag == "table") {
      inits.push_back(MakeTable(child, tree, fontSize, focus, sheet, style));
    } else if (tag == "input" && IsCheckableInputType(child)) {
      const std::string *type = child.GetAttribute("type");
      BoxKind boxKind = *type == "radio" ? BoxKind::kRadio : BoxKind::kCheckbox;
      bool checked = child.GetAttribute("checked") != nullptr;
      inits.push_back(MakeCheckable(tree, &child, style, boxKind, checked));
    } else if (tag == "input") {
      if (&child == focus.node) {
        // The rendered label falls back to the placeholder when `value`
        // is empty (InputLabel), but cursorPos/selectionAnchor are
        // offsets into `value` - clamp against value's actual length, not
        // the placeholder's, and pin to 0 when value is empty (a click
        // handler like Clear can shrink `value` out from under a caret
        // position computed against the old, longer text; unclamped, the
        // renderer's label.substr(cursorPos) would throw).
        const std::string *value = child.GetAttribute("value");
        int valueLen = value != nullptr ? static_cast<int>(value->size()) : 0;
        int cursorPos = valueLen > 0
                             ? std::clamp(focus.cursorPos, 0, valueLen)
                             : 0;
        int selectionAnchor =
            valueLen > 0 ? std::clamp(focus.selectionAnchor, 0, valueLen)
                         : 0;
        inits.push_back(MakeBox(tree, InputLabel(child), fontSize, &child,
                                 style, cursorPos, selectionAnchor));
      } else {
        inits.push_back(
            MakeBox(tree, InputLabel(child), fontSize, &child, style));
      }
    } else if (tag == "button") {
      inits.push_back(
          MakeBox(tree, FlattenText(child), fontSize, &child, style));
    } else if (tag == "a") {
      inits.push_back(MakeLink(tree, child, fontSize, style));
    } else if (tag == "label") {
      inits.push_back(MakeLabel(tree, child, fontSize, style));
    } else {
      inits.push_back(MakeContainer(child, tree, FontSizeForTag(tag),
                                     IsBlockTag(tag), focus, sheet, style));
    }
  }

  return inits;
}

} // namespace

std::unique_ptr<WidgetTree> BuildWidgetTree(const Node &root,
                                             const InputFocus &focus) {
  auto tree = std::make_unique<WidgetTree>();

  std::string cssText;
  CollectStyleText(root, cssText);
  StyleSheet sheet = StyleSheet::Parse(cssText);

  // `root` itself (the document/body element) never becomes a Widget of
  // its own (see the synthetic top-level container below) but a rule
  // like `body { color: ... }` should still seed inheritance for
  // everything under it, so it's resolved here rather than skipped.
  Declarations rootStyle = sheet.Resolve(root, Declarations{});

  std::vector<Widget> rootChildren = BuildChildrenInits(
      root, *tree, kDefaultFontSize, focus, sheet, rootStyle);
  int count = static_cast<int>(rootChildren.size());
  const Widget *childrenPtr = tree->StoreArray(std::move(rootChildren));

  tree->SetRoot(Widget{WidgetKind::kContainer,
                        false,
                        0.0f,
                        nullptr,
                        childrenPtr,
                        count,
                        nullptr,
                        0,
                        0.0f,
                        0.0f,
                        false,
                        1,
                        1});

  return tree;
}

const Widget *WidgetTree::StoreArray(std::vector<Widget> widgets) {
  if (widgets.empty()) {
    return nullptr;
  }
  auto owned = std::make_unique<std::vector<Widget>>(std::move(widgets));
  const Widget *ptr = owned->data();
  arrays_.push_back(std::move(owned));
  return ptr;
}

const char *WidgetTree::StoreString(const std::string &text) {
  auto owned = std::make_unique<std::string>(text);
  const char *ptr = owned->c_str();
  strings_.push_back(std::move(owned));
  return ptr;
}

} // namespace artisan
