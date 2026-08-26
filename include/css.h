#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dom_node.h"
#include "widget.h"

namespace artisan {

// A single compound selector, e.g. "div.card#hero" - a tag name (optional
// - empty matches any tag, i.e. universal), an id (optional), and any
// number of classes, ALL of which must match (an AND, not an OR). No
// combinators (descendant/child/sibling), no pseudo-classes, no attribute
// selectors. A comma-separated selector list in markup ("h1, .card")
// becomes multiple Selectors sharing one Rule's declarations, each
// competing for the cascade independently (see StyleSheet::Resolve).
struct Selector {
  std::string tag;                // Empty = any tag.
  std::string id;                 // Empty = no id constraint.
  std::vector<std::string> classes; // Must all be present on the element.
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
  // unset, not its parent's).
  Declarations Resolve(const Node &node, const Declarations &inherited) const;

private:
  std::vector<Rule> rules_;
};

// The other direction from StyleSheet::Resolve: given a selector string
// (the same bounded grammar Selector documents - one compound selector,
// no comma-lists/combinators) rather than a node, find node(s) it matches
// within `root`'s subtree (not including `root` itself), in document
// order. QuerySelector stops at the first match (nullptr if none);
// QuerySelectorAll collects every match. Backs document/node's
// querySelector(All) JS bindings (js_engine.cpp) - a snapshot at call
// time, not a live view, same simplification WidgetTree's own "one-shot,
// not a live view" comment already documents for a different tree.
Node *QuerySelector(Node &root, const std::string &selector);
std::vector<Node *> QuerySelectorAll(Node &root, const std::string &selector);

// Whether `node` itself (not its subtree) matches `selector` - same
// bounded grammar as QuerySelector above. Backs the JS/Go `matches`
// binding, and is what Closest below is built from.
bool ElementMatches(const Node &node, const std::string &selector);

// `node`, or its nearest ancestor (inclusive - `node` itself counts, per
// real DOM's closest()) that matches `selector`. nullptr if neither
// `node` nor anything above it matches.
Node *Closest(Node &node, const std::string &selector);

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
