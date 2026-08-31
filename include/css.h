#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "dom_node.h"
#include "widget.h"

namespace artisan {

// The relational operators an attribute selector's value can use - real
// CSS's `~=`/`^=`/`$=`/`*=`/`|=`, plus plain `=`. Only meaningful when
// AttributeSelector::value is set (a bare "[name]" - presence only -
// ignores this entirely). No case-insensitive "i" flag, a bounded subset
// same as the rest of this file's CSS support.
enum class AttributeOperator {
  kEquals,    // [name=value]  - exact match.
  kIncludes,  // [name~=value] - value is one of the attribute's
              // whitespace-separated tokens (the general form of what
              // `.class` does specifically for the "class" attribute).
  kPrefix,    // [name^=value] - attribute starts with value.
  kSuffix,    // [name$=value] - attribute ends with value.
  kSubstring, // [name*=value] - attribute contains value anywhere.
  kDashMatch, // [name|=value] - attribute equals value exactly, or
              // starts with value immediately followed by a hyphen
              // ("en" matches "en" or "en-US", not "english") - mainly
              // used for `lang`, its original purpose in real CSS.
};

// A single compound selector, e.g. "div.card#hero" - a tag name (optional
// - empty matches any tag, i.e. universal), an id (optional), any number
// of classes, attribute constraints, and structural pseudo-classes, ALL
// of which must match (an AND, not an OR).
struct AttributeSelector {
  std::string name;
  // nullopt = "[name]" (presence only, any value); set = "[name<op>value]"
  // per `op` above.
  std::optional<std::string> value;
  AttributeOperator op = AttributeOperator::kEquals;
  // The trailing `i` (or `I`) flag - "[name<op>value i]" - matches
  // `value` against the attribute case-insensitively (ASCII only, same
  // as this file's other lowercasing). Only meaningful alongside `value`
  // - meaningless (and never set) for a bare "[name]" presence check,
  // which never compares a value at all. No `s` (explicitly
  // case-*sensitive*) flag - this file has no case-insensitive default
  // for any attribute to opt out of in the first place.
  bool caseInsensitive = false;
};

enum class PseudoClassKind {
  kFirstChild,
  kLastChild,
  kNthChild,
  kNthOfType,
  kHover,
  kFocus,
  kFocusWithin,
  kNot,
};

// `::before`/`::after` (the legacy single-colon `:before`/`:after` forms
// parse identically - see ParsePseudoClassToken, css.cpp). Unlike every
// PseudoClassKind above, these don't describe a *condition* an element
// must meet to match a rule - they name which generated-content slot a
// rule's `content` declares text for, for a given real element, an
// entirely different kind of thing from the rest of this file's
// selector grammar. Real CSS restricts a pseudo-element to appearing
// only as the last component of a selector's last compound; this parser
// doesn't reject a malformed selector that puts one elsewhere, it just
// never looks anywhere but CompoundSelector::pseudoElement on
// `compounds.back()` (see StyleSheet::Resolve), so one anywhere else is
// silently inert - same non-strict-validation posture as a malformed
// :nth-child() argument elsewhere in this file.
enum class PseudoElementKind {
  kNone,
  kBefore,
  kAfter,
};

// Declared here (defined below) so PseudoClassSelector::notArgs can
// point to some - std::shared_ptr/vector only need a complete type
// where they're actually dereferenced/destroyed (css.cpp), not at this
// declaration.
struct CompoundSelector;
struct Selector;

// nthA/nthB implement "an+b" - :nth-child(2)/:nth-of-type(2) are
// {a=0, b=2}; the "even"/"odd" keyword forms are {a=2, b=0}/{a=2, b=1}.
// Only used when kind is kNthChild or kNthOfType. The two differ in what
// they count: kNthChild's index/count are among *all* element siblings;
// kNthOfType's are among only the siblings that share this element's own
// tag name (real CSS semantics - `p:nth-of-type(2)` is the second `<p>`
// among its siblings, not the second element overall) - see
// MatchesPseudoClass in css.cpp for where that split actually happens.
//
// notArgs is only used when kind is kNot: `:not(a, b)` parses "a, b" the
// same way a top-level comma-separated selector list is (full Selector
// chains, each with its own combinators - not just bare compounds), via
// ParseSelectorList, and matching negates whatever matching *any* of
// them would have (real CSS Level 4 semantics: `:not()`'s argument is a
// full selector list, not just a single simple selector). A shared_ptr
// (not a plain vector<Selector>) because Selector -> CompoundSelector ->
// vector<PseudoClassSelector> is a cycle back to this same struct -
// embedding one by value here would make the types recursively
// infinite-sized; shared_ptr is just this file's way of making a
// self-referential AST node copyable, not an ownership-sharing decision
// (nothing else ever points at the same vector).
struct PseudoClassSelector {
  PseudoClassKind kind;
  int nthA = 0;
  int nthB = 0;
  std::shared_ptr<std::vector<Selector>> notArgs;
};

struct CompoundSelector {
  std::string tag;                  // Empty = any tag.
  std::string id;                   // Empty = no id constraint.
  std::vector<std::string> classes; // Must all be present on the element.
  std::vector<AttributeSelector> attributes;
  std::vector<PseudoClassSelector> pseudoClasses;
  // See PseudoElementKind above - only ever consulted on a selector's
  // *last* compound (StyleSheet::Resolve). Doesn't add to this
  // compound's own specificity (real CSS gives a pseudo-element
  // type-selector-level weight; skipped here, same bounded-subset
  // philosophy as the rest of this file's specificity model - see
  // SpecificityOfCompound, css.cpp).
  PseudoElementKind pseudoElement = PseudoElementKind::kNone;
};

// The live mouse/focus state a match against `:hover`/`:focus` needs -
// everything else in this file (tag/id/class/attribute/structural
// pseudo-classes) is decided purely from the Node tree's own shape, but
// these two depend on state that lives outside the DOM entirely (which
// element the pointer happens to be over right now, which <input> has
// focus - main.cpp owns both). Default-constructed (nullptr/nullptr)
// means "nothing is hovered or focused", which is what every caller that
// has no live UI state to offer (the JS/Go query bindings below) passes
// implicitly via each function's default argument - `:hover`/`:focus`
// simply never match through those, same as the rest of this struct
// being irrelevant to a one-shot snapshot query.
struct PseudoClassState {
  // The node the pointer is currently positioned over, if any. Matches
  // `:hover` on this node AND every one of its ancestors (real CSS
  // semantics: hovering a child counts as hovering its containers too) -
  // see MatchesPseudoClass in css.cpp for the walk that implements that.
  const Node *hovered = nullptr;
  // The node that currently has focus, if any. Matches `:focus` on this
  // exact node only - `:focus` itself doesn't bubble to ancestors, but
  // `:focus-within` (same field, different pseudo-class - see
  // MatchesPseudoClass in css.cpp) matches this node and every one of
  // its ancestors, the same walk `:hover` does above.
  const Node *focused = nullptr;
};

// How two adjacent compound selectors in a chain relate - "div p" is
// {div, kDescendant, p}, "div > p" is {div, kChild, p}, etc.
enum class Combinator { kDescendant, kChild, kAdjacentSibling, kGeneralSibling };

// A full selector: a chain of compound selectors joined by combinators,
// e.g. "div.card > p" parses to compounds=[div.card, p],
// combinators=[kChild]. `compounds` is never empty; `compounds.back()`
// is the actual subject being matched/selected - everything before it
// (walked via `combinators`, right to left) constrains its ancestors/
// siblings instead. A selector with just one compound (the common case)
// has an empty `combinators` vector. A comma-separated selector list in
// markup ("h1, .card") becomes multiple Selectors sharing one Rule's
// declarations, each competing for the cascade independently (see
// StyleSheet::Resolve) - or, for QuerySelector/QuerySelectorAll/
// ElementMatches/Closest below, multiple Selectors any of which counts
// as a match (see ParseSelectorList in css.cpp).
struct Selector {
  std::vector<CompoundSelector> compounds;
  std::vector<Combinator> combinators; // size() == compounds.size() - 1
};

// What a rule (or a resolved element) actually specifies. hasXxx tells a
// property was set at all - distinct from Color{} (black) or false/1.0f
// happening to be the chosen value. Whether a given WidgetKind paints
// backgroundColor/borderColor is WidgetRenderer's call, not this struct's.
struct Declarations {
  bool hasColor = false;
  Color color;
  bool hasBold = false; // font-weight was specified at all.
  bool bold = false;    // ...and its resolved value.
  bool hasBackgroundColor = false;
  Color backgroundColor;
  bool hasBorderColor = false;
  Color borderColor;
  bool hasBorderWidth = false;
  float borderWidth = 1.0f;

  // Box model - kContainer only for now (see widget_renderer.cpp's
  // ContainerWidgetHandler). width/height clamp/override the container's
  // natural size; padding/margin inset it. Every one of these carries a
  // hasXxx flag despite 0 being a plausible "unset" value for
  // padding/margin too - StyleSheet::Resolve's per-property cascade
  // (one rule might set padding-top without mentioning padding-left,
  // say) and MergeInlineStyle (an inline style might only override one
  // side) both need to tell "this rule/inline style didn't mention this
  // property" apart from "...set it to 0", the same way every other
  // property here already does.
  bool hasWidth = false;
  float width = 0.0f;
  bool widthIsPercent = false; // Resolves against the parent's width at
                                // render time - see kImage's identical
                                // imageWidthIsPercent for the precedent.
  bool hasHeight = false; // Pixels only - no percentage height, same
                           // reasoning kImage's imageHeight already has:
                           // no stable containing-block height in this
                           // flow model.
  float height = 0.0f;
  bool hasPaddingTop = false;
  float paddingTop = 0.0f;
  bool hasPaddingRight = false;
  float paddingRight = 0.0f;
  bool hasPaddingBottom = false;
  float paddingBottom = 0.0f;
  bool hasPaddingLeft = false;
  float paddingLeft = 0.0f;
  bool hasMarginTop = false;
  float marginTop = 0.0f;
  bool hasMarginRight = false;
  float marginRight = 0.0f;
  bool hasMarginBottom = false;
  float marginBottom = 0.0f;
  bool hasMarginLeft = false;
  float marginLeft = 0.0f;

  // Flexbox (see DisplayMode etc. above) - kContainer only. Every one of
  // these needs its own hasXxx flag for the same reason padding/margin
  // do: one rule might set display:flex without mentioning
  // justify-content, so "unset" has to be distinguishable from "set to
  // this enum's first value" (kBlock/kRow/kFlexStart), not just implied
  // by a default-constructed enum.
  bool hasDisplay = false;
  DisplayMode display = DisplayMode::kBlock;
  bool hasFlexDirection = false;
  FlexDirection flexDirection = FlexDirection::kRow;
  bool hasJustifyContent = false;
  JustifyContent justifyContent = JustifyContent::kFlexStart;
  bool hasAlignItems = false;
  // Real CSS flexbox's own default - also, unrelatedly, real CSS Grid's
  // own default for the exact same property (see ParseDeclarations,
  // css.cpp, for why this one field serves both).
  AlignItems alignItems = AlignItems::kStretch;
  // justify-items - CSS Grid only, flexbox has no equivalent (see
  // ParseDeclarations, css.cpp). Real CSS's own default is `stretch`.
  bool hasJustifyItems = false;
  AlignItems justifyItems = AlignItems::kStretch;
  bool hasAlignContent = false;
  // kFlexStart, not real CSS's kStretch default - see Widget::alignContent
  // (widget.h) for why this engine deliberately defaults the *unset*
  // case to matching its own pre-existing no-align-content behavior
  // instead.
  AlignContent alignContent = AlignContent::kFlexStart;
  bool hasGap = false;
  float gap = 0.0f;
  bool hasFlexWrap = false;
  FlexWrap flexWrap = FlexWrap::kNowrap;

  // Flex *item* properties - see Widget's identical fields for why
  // these live in the same flat namespace as the container properties
  // above rather than a separate struct.
  bool hasFlexGrow = false;
  float flexGrow = 0.0f;
  bool hasFlexShrink = false;
  float flexShrink = 1.0f;
  bool hasFlexBasis = false;
  float flexBasis = 0.0f;

  // CSS Grid - see Widget's identical fields (widget.h) for this bounded
  // subset's scope. Unlike Widget's, these carry their own hasXxx flags
  // - same reason every other Declarations field does (a rule setting
  // grid-template-rows without mentioning grid-template-columns has to
  // be distinguishable from one that sets grid-template-columns to
  // empty, at cascade time - see StyleSheet::Resolve).
  bool hasGridTemplateColumns = false;
  std::vector<GridTrack> gridTemplateColumns;
  bool hasGridTemplateRows = false;
  std::vector<GridTrack> gridTemplateRows;
  bool hasGridTemplateAreas = false;
  std::vector<std::vector<std::string>> gridTemplateAreas;
  // grid-area - a plain string, no hasXxx flag: same as CompoundSelector's
  // tag/id (css.h, above), an empty string already unambiguously means
  // "not set" (real CSS has no way to write a meaningful empty area
  // name), so a separate presence flag would be redundant.
  std::string gridArea;
  // grid-column/grid-row - GridLinePlacement's own hasStart already
  // distinguishes "no explicit start line" from "start line 1", but a
  // rule can also set only a bare `span N` (hasStart stays false, span
  // != 1) - that's still a real, cascade-competing value a lower-
  // specificity rule's own placement could lose to, so this still needs
  // its own top-level hasXxx flag the same as every numeric/enum
  // property does, unlike gridArea's plain string above.
  bool hasGridColumn = false;
  GridLinePlacement gridColumn;
  bool hasGridRow = false;
  GridLinePlacement gridRow;

  // `content` - only meaningful when this Declarations was resolved for
  // a `::before`/`::after` pseudo-element (StyleSheet::Resolve's `target`
  // parameter), where it's the literal text a synthetic kText child gets
  // built from (widget_tree_builder.cpp's BuildChildrenInits) - real
  // CSS's `attr()`/`counter()`/open-quote/close-quote and any other
  // dynamic content forms aren't supported, only a plain quoted string
  // (`content: "..."` or `'...'`) or `content: none`, which resolves to
  // hasContent=true with an empty string - a real, cascade-competing
  // "no content" value (able to override a lower-specificity rule's
  // non-empty content), not the same as this property being unset
  // entirely. A quoted value shouldn't contain a literal `;` - like the
  // rest of this file, ParseDeclarations isn't a full CSS tokenizer, and
  // splits a rule body on `;` without knowing to skip one inside quotes.
  bool hasContent = false;
  std::string content;
};

struct Rule {
  std::vector<Selector> selectors;
  Declarations declarations;
};

// Every <style> block's concatenated text, parsed into a flat, ordered
// rule list - source order matters for the cascade (see Resolve). Parses
// a bounded CSS subset: simple selectors as above, and five properties
// (color, background-color, font-weight, border-color, border-width);
// anything else is silently ignored, same as a browser skipping a
// property/selector it doesn't understand.
class StyleSheet {
public:
  static StyleSheet Parse(const std::string &css);

  bool empty() const { return rules_.empty(); }

  // The declarations that apply to `node`: every matching selector across
  // every rule competes per-property (not per-rule) on (specificity,
  // source order) - id > class > tag > universal, and later rules win
  // ties, exactly like a real cascade. `inherited` supplies color/bold
  // for any property this element doesn't set itself (backgroundColor/
  // borderColor/borderWidth never inherit - an unset element is just
  // unset, not its parent's). `pseudoState` is what `:hover`/`:focus`
  // selectors match against - see PseudoClassState above; omit it (or
  // pass a default-constructed one) to resolve as if nothing is hovered
  // or focused. `target` picks which of `node`'s selectors even compete
  // at all: kNone (the default) considers only a selector whose last
  // compound has no pseudo-element, i.e. resolving `node` itself, same
  // as before this parameter existed; kBefore/kAfter instead considers
  // only a selector targeting that pseudo-element specifically (e.g.
  // `.card::before`), letting a caller resolve `node`'s generated
  // content the exact same way it resolves `node` itself - see
  // BuildChildrenInits, widget_tree_builder.cpp.
  Declarations Resolve(const Node &node, const Declarations &inherited,
                        const PseudoClassState &pseudoState = {},
                        PseudoElementKind target = PseudoElementKind::kNone) const;

private:
  std::vector<Rule> rules_;
};

// The other direction from StyleSheet::Resolve: given a selector string
// (the same grammar Selector documents - descendant/child/sibling
// combinators, attribute selectors, :first-child/:last-child/:nth-child,
// :hover/:focus, comma-separated lists) rather than a node, find node(s)
// it matches within `root`'s subtree (not including `root` itself), in
// document order. QuerySelector stops at the first match (nullptr if
// none); QuerySelectorAll collects every match. Backs document/node's
// querySelector(All) JS bindings (js_engine.cpp) - a snapshot at call
// time, not a live view, same simplification WidgetTree's own "one-shot,
// not a live view" comment already documents for a different tree.
// `pseudoState` defaults to "nothing hovered or focused" - none of these
// query entry points has live UI state to offer on its own, so a
// `:hover`/`:focus` selector simply never matches unless a caller that
// does have it (there isn't one today) passes it explicitly.
Node *QuerySelector(Node &root, const std::string &selector,
                     const PseudoClassState &pseudoState = {});
std::vector<Node *> QuerySelectorAll(Node &root, const std::string &selector,
                                      const PseudoClassState &pseudoState = {});

// Whether `node` itself (not its subtree) matches `selector` - same
// bounded grammar as QuerySelector above. Backs the JS/Go `matches`
// binding, and is what Closest below is built from.
bool ElementMatches(const Node &node, const std::string &selector,
                     const PseudoClassState &pseudoState = {});

// `node`, or its nearest ancestor (inclusive - `node` itself counts, per
// real DOM's closest()) that matches `selector`. nullptr if neither
// `node` nor anything above it matches.
Node *Closest(Node &node, const std::string &selector,
              const PseudoClassState &pseudoState = {});

// Folds `node`'s own inline `style="..."` attribute (if any) into
// `declarations`, parsed with the same ParseDeclarations a <style>
// block's rules use - so an inline style accepts exactly the same five
// properties (color/background-color/font-weight/border-color/
// border-width) and nothing more. Overrides whatever the stylesheet
// cascade already resolved: inline style is the highest-precedence
// layer in real CSS too, above even an #id selector. Called once per
// element by BuildWidgetTree (widget_tree_builder.cpp), right after
// StyleSheet::Resolve.
void MergeInlineStyle(const Node &node, Declarations &declarations);

// The raw string value of one property in `node`'s inline style
// attribute - std::nullopt if the attribute is unset or doesn't mention
// `property` (case-insensitive, matching <style> block property names).
// Backs the JS `style.color` etc. getters (js_engine.cpp) - deliberately
// returns the original text, not a re-serialized/normalized form (e.g.
// "RED" stays "RED"), same as a real CSSStyleDeclaration getter would
// for a value it can't be sure how to canonicalize.
std::optional<std::string> GetInlineStyleProperty(const Node &node,
                                                    const std::string &property);

// Sets one property in `node`'s inline style attribute to `value`
// (empty `value` removes it instead - matching real
// `element.style.color = ""`), preserving every other property already
// there and their original order. Read-modify-write on the attribute's
// raw "prop: value; prop2: value2" text.
void SetInlineStyleProperty(Node &node, const std::string &property,
                             const std::string &value);

} // namespace artisan
