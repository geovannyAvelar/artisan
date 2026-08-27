#include "css.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>

namespace artisan {

namespace {

std::string Trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

std::string ToLower(const std::string &s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                  [](unsigned char c) { return std::tolower(c); });
  return out;
}

// Removes /* ... */ comments so they can't be mistaken for selector or
// declaration text - CSS comments don't nest, so a plain non-greedy scan
// is enough.
std::string StripComments(const std::string &css) {
  std::string out;
  out.reserve(css.size());
  for (size_t i = 0; i < css.size();) {
    if (i + 1 < css.size() && css[i] == '/' && css[i + 1] == '*') {
      size_t end = css.find("*/", i + 2);
      i = (end == std::string::npos) ? css.size() : end + 2;
    } else {
      out += css[i];
      ++i;
    }
  }
  return out;
}

bool ParseColor(const std::string &raw, Color &out) {
  std::string value = Trim(raw);
  if (value.empty()) {
    return false;
  }

  if (value[0] == '#') {
    std::string hex = value.substr(1);
    if (hex.size() == 3) {
      std::string expanded;
      for (char c : hex) {
        expanded += c;
        expanded += c;
      }
      hex = expanded;
    }
    if (hex.size() != 6 ||
        !std::all_of(hex.begin(), hex.end(),
                      [](unsigned char c) { return std::isxdigit(c); })) {
      return false;
    }
    out.r = static_cast<unsigned char>(std::stoi(hex.substr(0, 2), nullptr, 16));
    out.g = static_cast<unsigned char>(std::stoi(hex.substr(2, 2), nullptr, 16));
    out.b = static_cast<unsigned char>(std::stoi(hex.substr(4, 2), nullptr, 16));
    return true;
  }

  std::string lower = ToLower(value);
  if (lower.rfind("rgb", 0) == 0) {
    size_t open = lower.find('(');
    size_t close = lower.find(')');
    if (open == std::string::npos || close == std::string::npos) {
      return false;
    }
    std::string inner = lower.substr(open + 1, close - open - 1);
    std::vector<int> nums;
    std::stringstream ss(inner);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
      try {
        nums.push_back(std::stoi(Trim(tok)));
      } catch (const std::exception &) {
        return false;
      }
    }
    if (nums.size() < 3) {
      return false;
    }
    out.r = static_cast<unsigned char>(std::clamp(nums[0], 0, 255));
    out.g = static_cast<unsigned char>(std::clamp(nums[1], 0, 255));
    out.b = static_cast<unsigned char>(std::clamp(nums[2], 0, 255));
    return true;
  }

  // A modest set of common CSS keyword colors - not the full ~150-name
  // list, just enough to be useful without a giant table.
  static const std::unordered_map<std::string, Color> kNamed = {
      {"black", {0, 0, 0}},       {"white", {255, 255, 255}},
      {"red", {255, 0, 0}},       {"green", {0, 128, 0}},
      {"blue", {0, 0, 255}},      {"yellow", {255, 255, 0}},
      {"orange", {255, 165, 0}},  {"purple", {128, 0, 128}},
      {"gray", {128, 128, 128}},  {"grey", {128, 128, 128}},
      {"pink", {255, 192, 203}},  {"brown", {165, 42, 42}},
      {"cyan", {0, 255, 255}},    {"magenta", {255, 0, 255}},
      {"navy", {0, 0, 128}},      {"teal", {0, 128, 128}},
      {"lime", {0, 255, 0}},      {"maroon", {128, 0, 0}},
      {"olive", {128, 128, 0}},   {"silver", {192, 192, 192}},
      {"gold", {255, 215, 0}},
  };
  auto it = kNamed.find(lower);
  if (it != kNamed.end()) {
    out = it->second;
    return true;
  }

  return false;
}

bool ParsePixelLength(const std::string &raw, float &out) {
  std::string value = Trim(raw);
  if (value.size() >= 2 && value.substr(value.size() - 2) == "px") {
    value = value.substr(0, value.size() - 2);
  }
  try {
    out = std::stof(value);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

// Like ParsePixelLength, but also accepts a trailing '%' (setting
// `outIsPercent`) - used by `width`, the one box-model property that
// resolves against a containing size at render time rather than always
// being an absolute pixel value (see Declarations::widthIsPercent).
bool ParseLengthOrPercent(const std::string &raw, float &outValue,
                           bool &outIsPercent) {
  std::string value = Trim(raw);
  if (!value.empty() && value.back() == '%') {
    try {
      outValue = std::stof(value.substr(0, value.size() - 1));
      outIsPercent = true;
      return true;
    } catch (const std::exception &) {
      return false;
    }
  }
  outIsPercent = false;
  return ParsePixelLength(value, outValue);
}

// Parses a pseudo-class token (the text after ':', e.g. "first-child",
// "nth-child(2)", "nth-child(even)") and appends it to `selector` -
// silently ignored (matching this file's "skip what we don't
// understand" philosophy elsewhere, e.g. ParseDeclarations) if it isn't
// one of the structural pseudo-classes this bounded grammar supports.
// nth-child's v1 grammar: a literal integer, or the "even"/"odd"
// keywords - not the full "an+b" algebra.
void ParsePseudoClassToken(const std::string &token, CompoundSelector &selector) {
  std::string lower = ToLower(token);
  PseudoClassSelector pc;
  if (lower == "first-child") {
    pc.kind = PseudoClassKind::kFirstChild;
  } else if (lower == "last-child") {
    pc.kind = PseudoClassKind::kLastChild;
  } else if (lower.rfind("nth-child(", 0) == 0 && !lower.empty() &&
             lower.back() == ')') {
    std::string arg = Trim(lower.substr(10, lower.size() - 10 - 1));
    pc.kind = PseudoClassKind::kNthChild;
    if (arg == "even") {
      pc.nthA = 2;
      pc.nthB = 0;
    } else if (arg == "odd") {
      pc.nthA = 2;
      pc.nthB = 1;
    } else {
      try {
        pc.nthA = 0;
        pc.nthB = std::stoi(arg);
      } catch (const std::exception &) {
        return;
      }
    }
  } else {
    return;
  }
  selector.pseudoClasses.push_back(pc);
}

// Splits a compound selector ("div.card#hero", ".a.b", "*", "h1",
// "a[href]", "li:nth-child(2)") into its tag/id/class/attribute/pseudo-
// class parts. The leading segment (before the first special marker) is
// the tag - absent or "*" means "any tag" (universal). Attribute/
// pseudo-class values are assumed free of whitespace and of the marker
// characters ('.', '#', '[', ':') themselves - same bounded-subset
// philosophy as ParseColor's rgb() parsing elsewhere in this file, not a
// full CSS tokenizer. Used both by StyleSheet::Parse's rule parsing and
// by ParseSelectorChain below (same grammar, different direction - see
// css.h).
CompoundSelector ParseCompoundSelector(const std::string &raw) {
  CompoundSelector selector;
  std::string value = Trim(raw);
  if (value.empty()) {
    return selector;
  }

  size_t pos = value.find_first_of(".#[:");
  std::string head = value.substr(0, pos);
  if (!head.empty() && head != "*") {
    selector.tag = ToLower(head);
  }

  while (pos != std::string::npos) {
    char marker = value[pos];

    if (marker == '[') {
      size_t close = value.find(']', pos + 1);
      std::string inner = value.substr(
          pos + 1, (close == std::string::npos ? value.size() : close) - pos - 1);
      size_t eq = inner.find('=');
      AttributeSelector attr;
      if (eq == std::string::npos) {
        attr.name = ToLower(Trim(inner));
      } else {
        attr.name = ToLower(Trim(inner.substr(0, eq)));
        std::string attrValue = Trim(inner.substr(eq + 1));
        if (attrValue.size() >= 2 &&
            (attrValue.front() == '"' || attrValue.front() == '\'') &&
            attrValue.back() == attrValue.front()) {
          attrValue = attrValue.substr(1, attrValue.size() - 2);
        }
        attr.value = attrValue;
      }
      if (!attr.name.empty()) {
        selector.attributes.push_back(std::move(attr));
      }
      pos = (close == std::string::npos)
                ? std::string::npos
                : value.find_first_of(".#[:", close + 1);
      continue;
    }

    size_t next = value.find_first_of(".#[:", pos + 1);
    std::string token =
        value.substr(pos + 1, (next == std::string::npos ? value.size()
                                                           : next) -
                                   pos - 1);
    if (marker == '.' && !token.empty()) {
      selector.classes.push_back(token);
    } else if (marker == '#' && !token.empty()) {
      selector.id = token;
    } else if (marker == ':' && !token.empty()) {
      ParsePseudoClassToken(token, selector);
    }
    pos = next;
  }

  return selector;
}

// Splits a full selector string ("div.card > p", "a + b", "div p") into
// a chain of compound-selector tokens and the combinator between each
// pair - an implicit descendant combinator is inserted between two
// compound tokens separated only by whitespace (no explicit >/+/~).
// Whitespace/combinator characters inside an unclosed '[' ... ']' don't
// split a token (so "[title=\"a b\"]" isn't torn in two, even though the
// bounded grammar inside the brackets - see ParseCompoundSelector -
// doesn't otherwise support a quoted value containing a space).
Selector ParseSelectorChain(const std::string &raw) {
  Selector selector;
  std::string text = Trim(raw);
  std::string currentToken;
  bool pendingCombinator = false;
  Combinator combinator = Combinator::kDescendant;
  int bracketDepth = 0;

  auto flushToken = [&]() {
    std::string trimmed = Trim(currentToken);
    if (!trimmed.empty()) {
      if (!selector.compounds.empty()) {
        selector.combinators.push_back(pendingCombinator ? combinator
                                                           : Combinator::kDescendant);
      }
      selector.compounds.push_back(ParseCompoundSelector(trimmed));
    }
    pendingCombinator = false;
    currentToken.clear();
  };

  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == '[') {
      ++bracketDepth;
      currentToken += c;
      continue;
    }
    if (c == ']') {
      if (bracketDepth > 0) {
        --bracketDepth;
      }
      currentToken += c;
      continue;
    }
    if (bracketDepth == 0 && (c == '>' || c == '+' || c == '~')) {
      flushToken();
      combinator = (c == '>')   ? Combinator::kChild
                   : (c == '+') ? Combinator::kAdjacentSibling
                                : Combinator::kGeneralSibling;
      pendingCombinator = true;
      continue;
    }
    if (bracketDepth == 0 && std::isspace(static_cast<unsigned char>(c))) {
      if (!currentToken.empty()) {
        flushToken();
      }
      continue;
    }
    currentToken += c;
  }
  flushToken();

  return selector;
}

// Splits a comma-separated selector list ("h1, .card") into independent
// Selectors, each parsed via ParseSelectorChain - shared by
// StyleSheet::Parse (a rule's selectors, ANY of which puts it in the
// cascade for a given element) and QuerySelector/QuerySelectorAll/
// ElementMatches/Closest below (ANY of which counts as a match).
// Discards a selector that parsed to zero compounds (empty/malformed
// input) rather than letting it match everything.
std::vector<Selector> ParseSelectorList(const std::string &raw) {
  std::vector<Selector> result;
  std::stringstream ss(raw);
  std::string part;
  while (std::getline(ss, part, ',')) {
    std::string trimmed = Trim(part);
    if (trimmed.empty()) {
      continue;
    }
    Selector selector = ParseSelectorChain(trimmed);
    if (!selector.compounds.empty()) {
      result.push_back(std::move(selector));
    }
  }
  return result;
}

Declarations ParseDeclarations(const std::string &body) {
  Declarations decl;
  std::stringstream ss(body);
  std::string statement;

  while (std::getline(ss, statement, ';')) {
    size_t colon = statement.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    std::string property = ToLower(Trim(statement.substr(0, colon)));
    std::string value = Trim(statement.substr(colon + 1));

    if (property == "color") {
      decl.hasColor = ParseColor(value, decl.color);
    } else if (property == "background-color") {
      decl.hasBackgroundColor = ParseColor(value, decl.backgroundColor);
    } else if (property == "font-weight") {
      std::string lower = ToLower(value);
      if (lower == "bold") {
        decl.hasBold = true;
        decl.bold = true;
      } else if (lower == "normal") {
        decl.hasBold = true;
        decl.bold = false;
      }
    } else if (property == "border-color") {
      decl.hasBorderColor = ParseColor(value, decl.borderColor);
    } else if (property == "border-width") {
      decl.hasBorderWidth = ParsePixelLength(value, decl.borderWidth);
    } else if (property == "width") {
      decl.hasWidth = ParseLengthOrPercent(value, decl.width, decl.widthIsPercent);
    } else if (property == "height") {
      decl.hasHeight = ParsePixelLength(value, decl.height);
    } else if (property == "padding") {
      // Single-value shorthand only (all four sides) - 2/3/4-value
      // shorthand splitting ("10px 20px") is explicitly deferred, same
      // bounded-subset philosophy as the rest of this parser.
      float px;
      if (ParsePixelLength(value, px)) {
        decl.hasPaddingTop = decl.hasPaddingRight = decl.hasPaddingBottom =
            decl.hasPaddingLeft = true;
        decl.paddingTop = decl.paddingRight = decl.paddingBottom = decl.paddingLeft = px;
      }
    } else if (property == "padding-top") {
      decl.hasPaddingTop = ParsePixelLength(value, decl.paddingTop);
    } else if (property == "padding-right") {
      decl.hasPaddingRight = ParsePixelLength(value, decl.paddingRight);
    } else if (property == "padding-bottom") {
      decl.hasPaddingBottom = ParsePixelLength(value, decl.paddingBottom);
    } else if (property == "padding-left") {
      decl.hasPaddingLeft = ParsePixelLength(value, decl.paddingLeft);
    } else if (property == "margin") {
      float px;
      if (ParsePixelLength(value, px)) {
        decl.hasMarginTop = decl.hasMarginRight = decl.hasMarginBottom =
            decl.hasMarginLeft = true;
        decl.marginTop = decl.marginRight = decl.marginBottom = decl.marginLeft = px;
      }
    } else if (property == "margin-top") {
      decl.hasMarginTop = ParsePixelLength(value, decl.marginTop);
    } else if (property == "margin-right") {
      decl.hasMarginRight = ParsePixelLength(value, decl.marginRight);
    } else if (property == "margin-bottom") {
      decl.hasMarginBottom = ParsePixelLength(value, decl.marginBottom);
    } else if (property == "margin-left") {
      decl.hasMarginLeft = ParsePixelLength(value, decl.marginLeft);
    } else if (property == "display") {
      std::string lower = ToLower(value);
      if (lower == "flex") {
        decl.hasDisplay = true;
        decl.display = DisplayMode::kFlex;
      } else if (lower == "block") {
        decl.hasDisplay = true;
        decl.display = DisplayMode::kBlock;
      }
    } else if (property == "flex-direction") {
      std::string lower = ToLower(value);
      if (lower == "row") {
        decl.hasFlexDirection = true;
        decl.flexDirection = FlexDirection::kRow;
      } else if (lower == "column") {
        decl.hasFlexDirection = true;
        decl.flexDirection = FlexDirection::kColumn;
      }
    } else if (property == "justify-content") {
      std::string lower = ToLower(value);
      if (lower == "flex-start") {
        decl.hasJustifyContent = true;
        decl.justifyContent = JustifyContent::kFlexStart;
      } else if (lower == "center") {
        decl.hasJustifyContent = true;
        decl.justifyContent = JustifyContent::kCenter;
      } else if (lower == "flex-end") {
        decl.hasJustifyContent = true;
        decl.justifyContent = JustifyContent::kFlexEnd;
      } else if (lower == "space-between") {
        decl.hasJustifyContent = true;
        decl.justifyContent = JustifyContent::kSpaceBetween;
      }
    } else if (property == "align-items") {
      std::string lower = ToLower(value);
      if (lower == "flex-start") {
        decl.hasAlignItems = true;
        decl.alignItems = AlignItems::kFlexStart;
      } else if (lower == "center") {
        decl.hasAlignItems = true;
        decl.alignItems = AlignItems::kCenter;
      } else if (lower == "flex-end") {
        decl.hasAlignItems = true;
        decl.alignItems = AlignItems::kFlexEnd;
      } else if (lower == "stretch") {
        decl.hasAlignItems = true;
        decl.alignItems = AlignItems::kStretch;
      }
    } else if (property == "gap") {
      decl.hasGap = ParsePixelLength(value, decl.gap);
    } else if (property == "flex-wrap") {
      std::string lower = ToLower(value);
      if (lower == "nowrap") {
        decl.hasFlexWrap = true;
        decl.flexWrap = FlexWrap::kNowrap;
      } else if (lower == "wrap") {
        decl.hasFlexWrap = true;
        decl.flexWrap = FlexWrap::kWrap;
      }
    } else if (property == "flex-grow") {
      decl.hasFlexGrow = ParsePixelLength(value, decl.flexGrow);
    } else if (property == "flex-shrink") {
      decl.hasFlexShrink = ParsePixelLength(value, decl.flexShrink);
    } else if (property == "flex-basis") {
      if (ToLower(Trim(value)) == "auto") {
        decl.hasFlexBasis = false;
      } else {
        decl.hasFlexBasis = ParsePixelLength(value, decl.flexBasis);
      }
    }
    // Any other property is silently ignored, same as a browser skipping
    // one it doesn't recognize.
  }

  return decl;
}

// Whether `node` is at the position `pc` describes among its parent's
// *element* siblings (1-based - real CSS's :nth-child counts only
// element children). A node with no parent (root/detached) is treated
// as its own only sibling, so :first-child/:last-child/:nth-child(1)
// all vacuously match it - same as real CSS resolves an unparented
// element.
bool MatchesPseudoClass(const PseudoClassSelector &pc, const Node &node) {
  const Node *parent = node.parent();
  // No parent (root/detached): vacuously node's own only sibling - same
  // as computing index=1, elementCount=1 below, just without a parent
  // to walk children() on.
  int index = 1;
  int elementCount = 1;

  if (parent != nullptr) {
    index = 0;
    elementCount = 0;
    for (const auto &siblingPtr : parent->children()) {
      const Node *sibling = siblingPtr.get();
      if (sibling->type() != NodeType::kElement) {
        continue;
      }
      ++elementCount;
      if (sibling == &node) {
        index = elementCount;
      }
    }
    if (index == 0) {
      return false;
    }
  }

  switch (pc.kind) {
  case PseudoClassKind::kFirstChild:
    return index == 1;
  case PseudoClassKind::kLastChild:
    return index == elementCount;
  case PseudoClassKind::kNthChild:
    // v1 grammar only ever produces nthA == 0 (a literal index) or
    // nthA == 2 (even/odd) - both non-negative, so plain modulo is safe
    // (no negative-operand sign concerns a general "an+b" would have).
    return pc.nthA == 0 ? index == pc.nthB : (index % pc.nthA) == pc.nthB;
  }
  return false;
}

// Every part of a compound selector must match - a tag constraint, an id
// constraint, every listed class, every attribute constraint, and every
// pseudo-class, all AND'd together (an empty part imposes no constraint,
// so a bare CompoundSelector{} - "*" - matches anything). The rightmost
// compound of a Selector chain - see MatchesChain below for the rest of
// the chain (ancestor/sibling compounds).
bool CompoundMatches(const CompoundSelector &selector, const Node &node) {
  if (node.type() != NodeType::kElement) {
    return false;
  }

  if (!selector.tag.empty() && node.tagName() != selector.tag) {
    return false;
  }

  if (!selector.id.empty()) {
    const std::string *id = node.GetAttribute("id");
    if (id == nullptr || *id != selector.id) {
      return false;
    }
  }

  if (!selector.classes.empty()) {
    const std::string *classAttr = node.GetAttribute("class");
    if (classAttr == nullptr) {
      return false;
    }
    std::vector<std::string> nodeClasses;
    std::stringstream ss(*classAttr);
    std::string token;
    while (ss >> token) {
      nodeClasses.push_back(token);
    }
    for (const std::string &want : selector.classes) {
      if (std::find(nodeClasses.begin(), nodeClasses.end(), want) ==
          nodeClasses.end()) {
        return false;
      }
    }
  }

  for (const AttributeSelector &attr : selector.attributes) {
    const std::string *value = node.GetAttribute(attr.name);
    if (value == nullptr) {
      return false;
    }
    if (attr.value.has_value() && *value != *attr.value) {
      return false;
    }
  }

  for (const PseudoClassSelector &pc : selector.pseudoClasses) {
    if (!MatchesPseudoClass(pc, node)) {
      return false;
    }
  }

  return true;
}

// Verifies compounds[0..upToIndex] against ancestors/siblings of
// `contextNode` (the node the compound at upToIndex+1 already matched),
// walking leftward through `selector.combinators`. kDescendant
// backtracks - if the rest of the chain doesn't pan out from the first
// ancestor that matches this compound, it retries the next ancestor up,
// rather than committing to the first match (real CSS semantics: "div p
// b" must match even when there are two nested <p> ancestors and only
// the outer one has a "div" ancestor of its own).
bool MatchesAncestorChain(const Selector &selector, int upToIndex,
                           const Node *contextNode) {
  if (upToIndex < 0) {
    return true;
  }

  const CompoundSelector &compound = selector.compounds[upToIndex];
  switch (selector.combinators[upToIndex]) {
  case Combinator::kDescendant:
    for (const Node *ancestor = contextNode->parent(); ancestor != nullptr;
         ancestor = ancestor->parent()) {
      if (CompoundMatches(compound, *ancestor) &&
          MatchesAncestorChain(selector, upToIndex - 1, ancestor)) {
        return true;
      }
    }
    return false;
  case Combinator::kChild: {
    const Node *parent = contextNode->parent();
    return parent != nullptr && CompoundMatches(compound, *parent) &&
           MatchesAncestorChain(selector, upToIndex - 1, parent);
  }
  case Combinator::kAdjacentSibling: {
    const Node *prev = contextNode->previousSibling();
    return prev != nullptr && CompoundMatches(compound, *prev) &&
           MatchesAncestorChain(selector, upToIndex - 1, prev);
  }
  case Combinator::kGeneralSibling:
    for (const Node *prev = contextNode->previousSibling(); prev != nullptr;
         prev = prev->previousSibling()) {
      if (CompoundMatches(compound, *prev) &&
          MatchesAncestorChain(selector, upToIndex - 1, prev)) {
        return true;
      }
    }
    return false;
  }
  return false;
}

// Whether `node` matches the full selector chain - the rightmost
// compound against `node` itself, then each earlier compound against an
// ancestor/sibling per its combinator (MatchesAncestorChain above).
bool MatchesChain(const Selector &selector, const Node &node) {
  if (selector.compounds.empty()) {
    return false;
  }
  if (!CompoundMatches(selector.compounds.back(), node)) {
    return false;
  }
  return MatchesAncestorChain(selector, static_cast<int>(selector.compounds.size()) - 2,
                               &node);
}

int SpecificityOfCompound(const CompoundSelector &compound) {
  int score = 0;
  if (!compound.id.empty()) {
    score += 100;
  }
  score += static_cast<int>(compound.classes.size()) * 10;
  score += static_cast<int>(compound.attributes.size()) * 10;
  score += static_cast<int>(compound.pseudoClasses.size()) * 10;
  if (!compound.tag.empty()) {
    score += 1;
  }
  return score;
}

// Generous per-tier multipliers (id=100, class/attribute/pseudo-class=10,
// tag=1) keep id/class/tag as cleanly separated tiers for any realistic
// selector - same relative ordering real CSS specificity gives - summed
// across every compound in the chain (a combinator itself contributes
// nothing, matching real CSS).
int Specificity(const Selector &selector) {
  int score = 0;
  for (const CompoundSelector &compound : selector.compounds) {
    score += SpecificityOfCompound(compound);
  }
  return score;
}

// Tracks the best (specificity, source order) seen so far for one
// property, so Resolve can decide whether a new candidate should win.
struct PropertyWinner {
  int specificity = -1;
  int ruleIndex = -1;

  bool ShouldTake(int candidateSpecificity, int candidateRuleIndex) {
    if (candidateSpecificity > specificity ||
        (candidateSpecificity == specificity &&
         candidateRuleIndex > ruleIndex)) {
      specificity = candidateSpecificity;
      ruleIndex = candidateRuleIndex;
      return true;
    }
    return false;
  }
};

} // namespace

// Depth-first walk of `root`'s subtree (not including `root` itself - a
// real querySelector never matches the element you called it on) testing
// each element against every selector in `selectors` (a comma-separated
// list - a match against ANY of them counts, same as real CSS), calling
// ParseSelectorList/MatchesChain above - fine to call from out here
// despite them being anonymous-namespace-local (that only restricts
// visibility to other translation units, not within this one, and this
// is textually after their definitions). Shared by QuerySelector (stops
// at the first match) and QuerySelectorAll (collects every match, in
// document order) below. Takes/hands back mutable Node* throughout
// despite children() only ever returning a const vector reference:
// unique_ptr::get() doesn't propagate that constness to the pointee (a
// standard library quirk, not one of this codebase's own choices), so
// `childPtr.get()` is already Node*, not const Node*.
namespace {
template <typename Callback>
void ForEachMatch(const std::vector<Selector> &selectors, Node &root,
                   const Callback &callback) {
  for (const auto &childPtr : root.children()) {
    Node *child = childPtr.get();
    bool matched = false;
    for (const Selector &selector : selectors) {
      if (MatchesChain(selector, *child)) {
        matched = true;
        break;
      }
    }
    if (matched) {
      if (!callback(child)) {
        return;
      }
    }
    ForEachMatch(selectors, *child, callback);
  }
}
} // namespace

Node *QuerySelector(Node &root, const std::string &selector) {
  std::vector<Selector> parsed = ParseSelectorList(selector);
  Node *found = nullptr;
  ForEachMatch(parsed, root, [&](Node *match) {
    found = match;
    return false; // Stop at the first match.
  });
  return found;
}

std::vector<Node *> QuerySelectorAll(Node &root, const std::string &selector) {
  std::vector<Selector> parsed = ParseSelectorList(selector);
  std::vector<Node *> results;
  ForEachMatch(parsed, root, [&](Node *match) {
    results.push_back(match);
    return true; // Keep going - collect every match.
  });
  return results;
}

bool ElementMatches(const Node &node, const std::string &selector) {
  for (const Selector &parsed : ParseSelectorList(selector)) {
    if (MatchesChain(parsed, node)) {
      return true;
    }
  }
  return false;
}

Node *Closest(Node &node, const std::string &selector) {
  std::vector<Selector> parsed = ParseSelectorList(selector);
  for (Node *n = &node; n != nullptr; n = n->parent()) {
    for (const Selector &selector : parsed) {
      if (MatchesChain(selector, *n)) {
        return n;
      }
    }
  }
  return nullptr;
}

void MergeInlineStyle(const Node &node, Declarations &declarations) {
  const std::string *styleAttr = node.GetAttribute("style");
  if (styleAttr == nullptr) {
    return;
  }
  Declarations inlineStyle = ParseDeclarations(*styleAttr);
  if (inlineStyle.hasColor) {
    declarations.hasColor = true;
    declarations.color = inlineStyle.color;
  }
  if (inlineStyle.hasBold) {
    declarations.hasBold = true;
    declarations.bold = inlineStyle.bold;
  }
  if (inlineStyle.hasBackgroundColor) {
    declarations.hasBackgroundColor = true;
    declarations.backgroundColor = inlineStyle.backgroundColor;
  }
  if (inlineStyle.hasBorderColor) {
    declarations.hasBorderColor = true;
    declarations.borderColor = inlineStyle.borderColor;
  }
  if (inlineStyle.hasBorderWidth) {
    declarations.hasBorderWidth = true;
    declarations.borderWidth = inlineStyle.borderWidth;
  }
  if (inlineStyle.hasWidth) {
    declarations.hasWidth = true;
    declarations.width = inlineStyle.width;
    declarations.widthIsPercent = inlineStyle.widthIsPercent;
  }
  if (inlineStyle.hasHeight) {
    declarations.hasHeight = true;
    declarations.height = inlineStyle.height;
  }
  if (inlineStyle.hasPaddingTop) {
    declarations.hasPaddingTop = true;
    declarations.paddingTop = inlineStyle.paddingTop;
  }
  if (inlineStyle.hasPaddingRight) {
    declarations.hasPaddingRight = true;
    declarations.paddingRight = inlineStyle.paddingRight;
  }
  if (inlineStyle.hasPaddingBottom) {
    declarations.hasPaddingBottom = true;
    declarations.paddingBottom = inlineStyle.paddingBottom;
  }
  if (inlineStyle.hasPaddingLeft) {
    declarations.hasPaddingLeft = true;
    declarations.paddingLeft = inlineStyle.paddingLeft;
  }
  if (inlineStyle.hasMarginTop) {
    declarations.hasMarginTop = true;
    declarations.marginTop = inlineStyle.marginTop;
  }
  if (inlineStyle.hasMarginRight) {
    declarations.hasMarginRight = true;
    declarations.marginRight = inlineStyle.marginRight;
  }
  if (inlineStyle.hasMarginBottom) {
    declarations.hasMarginBottom = true;
    declarations.marginBottom = inlineStyle.marginBottom;
  }
  if (inlineStyle.hasMarginLeft) {
    declarations.hasMarginLeft = true;
    declarations.marginLeft = inlineStyle.marginLeft;
  }
  if (inlineStyle.hasDisplay) {
    declarations.hasDisplay = true;
    declarations.display = inlineStyle.display;
  }
  if (inlineStyle.hasFlexDirection) {
    declarations.hasFlexDirection = true;
    declarations.flexDirection = inlineStyle.flexDirection;
  }
  if (inlineStyle.hasJustifyContent) {
    declarations.hasJustifyContent = true;
    declarations.justifyContent = inlineStyle.justifyContent;
  }
  if (inlineStyle.hasAlignItems) {
    declarations.hasAlignItems = true;
    declarations.alignItems = inlineStyle.alignItems;
  }
  if (inlineStyle.hasGap) {
    declarations.hasGap = true;
    declarations.gap = inlineStyle.gap;
  }
  if (inlineStyle.hasFlexWrap) {
    declarations.hasFlexWrap = true;
    declarations.flexWrap = inlineStyle.flexWrap;
  }
  if (inlineStyle.hasFlexGrow) {
    declarations.hasFlexGrow = true;
    declarations.flexGrow = inlineStyle.flexGrow;
  }
  if (inlineStyle.hasFlexShrink) {
    declarations.hasFlexShrink = true;
    declarations.flexShrink = inlineStyle.flexShrink;
  }
  if (inlineStyle.hasFlexBasis) {
    declarations.hasFlexBasis = true;
    declarations.flexBasis = inlineStyle.flexBasis;
  }
}

namespace {

// Parses "prop: value; prop2: value2" into an ordered list (not a map) -
// order is preserved so SerializeStyleText below round-trips a style
// attribute's property order the way a browser would, rather than
// reshuffling it alphabetically on every write.
std::vector<std::pair<std::string, std::string>>
ParseStyleProperties(const std::string &text) {
  std::vector<std::pair<std::string, std::string>> result;
  std::stringstream ss(text);
  std::string statement;
  while (std::getline(ss, statement, ';')) {
    size_t colon = statement.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    std::string property = ToLower(Trim(statement.substr(0, colon)));
    std::string value = Trim(statement.substr(colon + 1));
    if (!property.empty()) {
      result.emplace_back(std::move(property), std::move(value));
    }
  }
  return result;
}

std::string
SerializeStyleProperties(const std::vector<std::pair<std::string, std::string>> &props) {
  std::string out;
  for (const auto &[property, value] : props) {
    if (!out.empty()) {
      out += ' ';
    }
    out += property;
    out += ": ";
    out += value;
    out += ';';
  }
  return out;
}

} // namespace

std::optional<std::string> GetInlineStyleProperty(const Node &node,
                                                    const std::string &property) {
  const std::string *styleAttr = node.GetAttribute("style");
  if (styleAttr == nullptr) {
    return std::nullopt;
  }
  std::string wanted = ToLower(property);
  for (const auto &[name, value] : ParseStyleProperties(*styleAttr)) {
    if (name == wanted) {
      return value;
    }
  }
  return std::nullopt;
}

void SetInlineStyleProperty(Node &node, const std::string &property,
                             const std::string &value) {
  const std::string *styleAttr = node.GetAttribute("style");
  std::vector<std::pair<std::string, std::string>> props =
      styleAttr != nullptr ? ParseStyleProperties(*styleAttr)
                            : std::vector<std::pair<std::string, std::string>>{};

  std::string name = ToLower(property);
  auto it = std::find_if(props.begin(), props.end(),
                          [&](const auto &p) { return p.first == name; });

  if (value.empty()) {
    if (it != props.end()) {
      props.erase(it);
    }
  } else if (it != props.end()) {
    it->second = value;
  } else {
    props.emplace_back(name, value);
  }

  if (props.empty()) {
    node.RemoveAttribute("style");
  } else {
    node.SetAttribute("style", SerializeStyleProperties(props));
  }
}

StyleSheet StyleSheet::Parse(const std::string &css) {
  StyleSheet sheet;
  std::string text = StripComments(css);

  size_t pos = 0;
  while (pos < text.size()) {
    size_t open = text.find('{', pos);
    if (open == std::string::npos) {
      break;
    }
    size_t close = text.find('}', open);
    if (close == std::string::npos) {
      break;
    }

    std::string selectorText = text.substr(pos, open - pos);
    std::string body = text.substr(open + 1, close - open - 1);

    Rule rule;
    rule.declarations = ParseDeclarations(body);
    rule.selectors = ParseSelectorList(selectorText);

    if (!rule.selectors.empty()) {
      sheet.rules_.push_back(std::move(rule));
    }

    pos = close + 1;
  }

  return sheet;
}

Declarations StyleSheet::Resolve(const Node &node,
                                  const Declarations &inherited) const {
  Declarations own;

  PropertyWinner colorWin, boldWin, bgWin, borderColorWin, borderWidthWin;
  PropertyWinner widthWin, heightWin;
  PropertyWinner paddingTopWin, paddingRightWin, paddingBottomWin, paddingLeftWin;
  PropertyWinner marginTopWin, marginRightWin, marginBottomWin, marginLeftWin;
  PropertyWinner displayWin, flexDirectionWin, justifyContentWin, alignItemsWin, gapWin;
  PropertyWinner flexWrapWin, flexGrowWin, flexShrinkWin, flexBasisWin;

  for (size_t i = 0; i < rules_.size(); ++i) {
    const Rule &rule = rules_[i];
    int ruleIndex = static_cast<int>(i);

    int matchedSpecificity = -1;
    for (const Selector &selector : rule.selectors) {
      if (MatchesChain(selector, node)) {
        matchedSpecificity = std::max(matchedSpecificity, Specificity(selector));
      }
    }
    if (matchedSpecificity < 0) {
      continue;
    }

    if (rule.declarations.hasColor &&
        colorWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasColor = true;
      own.color = rule.declarations.color;
    }
    if (rule.declarations.hasBold &&
        boldWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasBold = true;
      own.bold = rule.declarations.bold;
    }
    if (rule.declarations.hasBackgroundColor &&
        bgWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasBackgroundColor = true;
      own.backgroundColor = rule.declarations.backgroundColor;
    }
    if (rule.declarations.hasBorderColor &&
        borderColorWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasBorderColor = true;
      own.borderColor = rule.declarations.borderColor;
    }
    if (rule.declarations.hasBorderWidth &&
        borderWidthWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasBorderWidth = true;
      own.borderWidth = rule.declarations.borderWidth;
    }
    if (rule.declarations.hasWidth &&
        widthWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasWidth = true;
      own.width = rule.declarations.width;
      own.widthIsPercent = rule.declarations.widthIsPercent;
    }
    if (rule.declarations.hasHeight &&
        heightWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasHeight = true;
      own.height = rule.declarations.height;
    }
    if (rule.declarations.hasPaddingTop &&
        paddingTopWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasPaddingTop = true;
      own.paddingTop = rule.declarations.paddingTop;
    }
    if (rule.declarations.hasPaddingRight &&
        paddingRightWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasPaddingRight = true;
      own.paddingRight = rule.declarations.paddingRight;
    }
    if (rule.declarations.hasPaddingBottom &&
        paddingBottomWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasPaddingBottom = true;
      own.paddingBottom = rule.declarations.paddingBottom;
    }
    if (rule.declarations.hasPaddingLeft &&
        paddingLeftWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasPaddingLeft = true;
      own.paddingLeft = rule.declarations.paddingLeft;
    }
    if (rule.declarations.hasMarginTop &&
        marginTopWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasMarginTop = true;
      own.marginTop = rule.declarations.marginTop;
    }
    if (rule.declarations.hasMarginRight &&
        marginRightWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasMarginRight = true;
      own.marginRight = rule.declarations.marginRight;
    }
    if (rule.declarations.hasMarginBottom &&
        marginBottomWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasMarginBottom = true;
      own.marginBottom = rule.declarations.marginBottom;
    }
    if (rule.declarations.hasMarginLeft &&
        marginLeftWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasMarginLeft = true;
      own.marginLeft = rule.declarations.marginLeft;
    }
    if (rule.declarations.hasDisplay &&
        displayWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasDisplay = true;
      own.display = rule.declarations.display;
    }
    if (rule.declarations.hasFlexDirection &&
        flexDirectionWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasFlexDirection = true;
      own.flexDirection = rule.declarations.flexDirection;
    }
    if (rule.declarations.hasJustifyContent &&
        justifyContentWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasJustifyContent = true;
      own.justifyContent = rule.declarations.justifyContent;
    }
    if (rule.declarations.hasAlignItems &&
        alignItemsWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasAlignItems = true;
      own.alignItems = rule.declarations.alignItems;
    }
    if (rule.declarations.hasGap && gapWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasGap = true;
      own.gap = rule.declarations.gap;
    }
    if (rule.declarations.hasFlexWrap &&
        flexWrapWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasFlexWrap = true;
      own.flexWrap = rule.declarations.flexWrap;
    }
    if (rule.declarations.hasFlexGrow &&
        flexGrowWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasFlexGrow = true;
      own.flexGrow = rule.declarations.flexGrow;
    }
    if (rule.declarations.hasFlexShrink &&
        flexShrinkWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasFlexShrink = true;
      own.flexShrink = rule.declarations.flexShrink;
    }
    if (rule.declarations.hasFlexBasis &&
        flexBasisWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasFlexBasis = true;
      own.flexBasis = rule.declarations.flexBasis;
    }
  }

  // color/font-weight inherit from the parent when this element doesn't
  // set its own - background/border never do (see Declarations).
  if (!own.hasColor && inherited.hasColor) {
    own.hasColor = true;
    own.color = inherited.color;
  }
  if (!own.hasBold && inherited.hasBold) {
    own.hasBold = true;
    own.bold = inherited.bold;
  }

  return own;
}

} // namespace artisan
