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

// Splits a compound selector ("div.card#hero", ".a.b", "*", "h1") into its
// tag/id/class parts. The leading segment (before the first '.'/'#') is
// the tag - absent or "*" means "any tag" (universal). Used both by
// StyleSheet::Parse's rule parsing and by QuerySelector/QuerySelectorAll
// below (same grammar, different direction - see css.h).
Selector ParseSelector(const std::string &raw) {
  Selector selector;
  std::string value = Trim(raw);
  if (value.empty()) {
    return selector;
  }

  size_t pos = value.find_first_of(".#");
  std::string head = value.substr(0, pos);
  if (!head.empty() && head != "*") {
    selector.tag = ToLower(head);
  }

  while (pos != std::string::npos) {
    char marker = value[pos];
    size_t next = value.find_first_of(".#", pos + 1);
    std::string token =
        value.substr(pos + 1, (next == std::string::npos ? value.size()
                                                           : next) -
                                   pos - 1);
    if (marker == '.' && !token.empty()) {
      selector.classes.push_back(token);
    } else if (marker == '#' && !token.empty()) {
      selector.id = token;
    }
    pos = next;
  }

  return selector;
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
    }
    // Any other property is silently ignored, same as a browser skipping
    // one it doesn't recognize.
  }

  return decl;
}

// Every part of a compound selector must match - a tag constraint, an id
// constraint, and every listed class, all AND'd together (an empty part
// imposes no constraint, so a bare Selector{} - "*" - matches anything).
// Used both by StyleSheet::Resolve's cascade and by QuerySelector/
// QuerySelectorAll below, in the opposite direction (see css.h).
bool Matches(const Selector &selector, const Node &node) {
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

  return true;
}

// Generous per-tier multipliers (id=100, class=10, tag=1) keep id/class/
// tag as cleanly separated tiers for any realistic number of classes on
// one selector - same relative ordering real CSS specificity gives.
int Specificity(const Selector &selector) {
  int score = 0;
  if (!selector.id.empty()) {
    score += 100;
  }
  score += static_cast<int>(selector.classes.size()) * 10;
  if (!selector.tag.empty()) {
    score += 1;
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
// each element against `selector`, calling ParseSelector/Matches above -
// fine to call from out here despite them being anonymous-namespace-local
// (that only restricts visibility to other translation units, not within
// this one, and this is textually after their definitions). Shared by
// QuerySelector (stops at the first match) and QuerySelectorAll (collects
// every match, in document order) below. Takes/hands back mutable Node*
// throughout despite children() only ever returning a const vector
// reference: unique_ptr::get() doesn't propagate that constness to the
// pointee (a standard library quirk, not one of this codebase's own
// choices), so `childPtr.get()` is already Node*, not const Node*.
namespace {
template <typename Callback>
void ForEachMatch(const Selector &selector, Node &root,
                   const Callback &callback) {
  for (const auto &childPtr : root.children()) {
    Node *child = childPtr.get();
    if (Matches(selector, *child)) {
      if (!callback(child)) {
        return;
      }
    }
    ForEachMatch(selector, *child, callback);
  }
}
} // namespace

Node *QuerySelector(Node &root, const std::string &selector) {
  Selector parsed = ParseSelector(selector);
  Node *found = nullptr;
  ForEachMatch(parsed, root, [&](Node *match) {
    found = match;
    return false; // Stop at the first match.
  });
  return found;
}

std::vector<Node *> QuerySelectorAll(Node &root, const std::string &selector) {
  Selector parsed = ParseSelector(selector);
  std::vector<Node *> results;
  ForEachMatch(parsed, root, [&](Node *match) {
    results.push_back(match);
    return true; // Keep going - collect every match.
  });
  return results;
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

    std::stringstream ss(selectorText);
    std::string selectorPart;
    while (std::getline(ss, selectorPart, ',')) {
      std::string trimmed = Trim(selectorPart);
      if (!trimmed.empty()) {
        rule.selectors.push_back(ParseSelector(trimmed));
      }
    }

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

  for (size_t i = 0; i < rules_.size(); ++i) {
    const Rule &rule = rules_[i];
    int ruleIndex = static_cast<int>(i);

    int matchedSpecificity = -1;
    for (const Selector &selector : rule.selectors) {
      if (Matches(selector, node)) {
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
