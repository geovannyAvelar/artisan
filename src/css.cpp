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

// Parses one non-repeat() track token - a plain pixel length (bare
// number or "...px"), an "Nfr" fractional unit (e.g. "2fr"), or the
// `min-content`/`max-content` keywords - see GridTrack (widget.h) for
// what each becomes. A token matching none of these forms is silently
// skipped, same "parse whatever we understand" posture
// ParseDeclarations already has for an unrecognized property.
void ParseGridTrackToken(const std::string &token, std::vector<GridTrack> &tracks) {
  std::string lower = ToLower(token);
  if (lower == "min-content") {
    tracks.push_back({GridTrackKind::kMinContent, 0.0f});
    return;
  }
  if (lower == "max-content") {
    tracks.push_back({GridTrackKind::kMaxContent, 0.0f});
    return;
  }
  if (lower.size() >= 3 && lower.substr(lower.size() - 2) == "fr") {
    try {
      tracks.push_back(
          {GridTrackKind::kFraction, std::stof(lower.substr(0, lower.size() - 2))});
    } catch (const std::exception &) {
      // Not a valid "Nfr" token - skip it.
    }
    return;
  }
  float px;
  if (ParsePixelLength(token, px)) {
    tracks.push_back({GridTrackKind::kFixed, px});
  }
}

// Scans a balanced parenthesized group whose opening '(' is at
// raw[openIndex], returning the index just past its matching close
// paren, or std::string::npos if it's never balanced - shared by
// repeat()/minmax()'s own paren-aware scanning in ParseGridTrackList
// below, so a comma or space inside either one's own argument list
// isn't mistaken for a track-list-level token boundary the way every
// other track already is.
size_t ScanBalancedParens(const std::string &raw, size_t openIndex) {
  size_t depth = 1;
  size_t j = openIndex + 1;
  while (j < raw.size() && depth > 0) {
    if (raw[j] == '(') {
      depth++;
    } else if (raw[j] == ')') {
      depth--;
    }
    j++;
  }
  return depth == 0 ? j : std::string::npos;
}

// Parses minmax()'s own two comma-separated arguments (already
// extracted, paren-stripped, e.g. "100px, 1fr") into a single kMinMax
// GridTrack - see GridTrack's own doc comment (widget.h) for the full
// contract. The min half is always a fixed px floor here (an
// unparseable one, e.g. real CSS's own min-content/max-content/auto for
// this half, silently leaves it at 0 - no floor at all, same "parse
// whatever we understand" posture as everywhere else in this file); the
// max half reuses ParseGridTrackToken - the same set every other track
// accepts, which also means neither minmax() nor repeat() nest inside
// this one's max half, since ParseGridTrackToken doesn't recognize
// either form. Returns false (out untouched) if there's no comma, or
// the max half doesn't parse to anything.
bool ParseMinMaxTrack(const std::string &inner, GridTrack &out) {
  size_t comma = inner.find(',');
  if (comma == std::string::npos) {
    return false;
  }
  float minPx = 0.0f;
  ParsePixelLength(Trim(inner.substr(0, comma)), minPx);
  std::vector<GridTrack> maxTracks;
  ParseGridTrackToken(Trim(inner.substr(comma + 1)), maxTracks);
  if (maxTracks.empty()) {
    return false;
  }
  out.kind = GridTrackKind::kMinMax;
  out.value = minPx;
  out.minMaxMaxKind = maxTracks[0].kind;
  out.minMaxMaxValue = maxTracks[0].value;
  return true;
}

// Parses a grid-template-columns/rows track list - space-separated
// tokens as ParseGridTrackToken understands them, plus `repeat(N,
// track-list)` (integer N only - `auto-fill`/`auto-fit` need to know
// how much space is available to size the repeat count, which isn't
// known yet at parse time, so they're left unsupported and the whole
// repeat() is skipped, same as any other unrecognized token) expanding
// inline to N copies of its inner track list at that position, and
// `minmax(min, max)` (see ParseMinMaxTrack above) as a single track at
// that position. Nesting repeat()/minmax() inside repeat()'s own inner
// list isn't supported (real CSS itself only allows repeat-inside-
// repeat in the auto-fill/auto-fit form this engine already excludes) -
// an inner list containing either is skipped whole, same as any other
// unrecognized token. Real CSS's `auto`/percentage tracks and named
// lines still aren't supported. Returns false (leaving `out` untouched)
// only when nothing in `raw` parsed to a single valid track.
bool ParseGridTrackList(const std::string &raw, std::vector<GridTrack> &out) {
  std::vector<GridTrack> tracks;
  size_t i = 0;
  while (i < raw.size()) {
    if (std::isspace(static_cast<unsigned char>(raw[i]))) {
      i++;
      continue;
    }
    if (raw.size() - i >= 7 && ToLower(raw.substr(i, 7)) == "repeat(") {
      size_t j = ScanBalancedParens(raw, i + 6);
      if (j != std::string::npos) {
        std::string inner = raw.substr(i + 7, (j - 1) - (i + 7));
        size_t comma = inner.find(',');
        if (comma != std::string::npos) {
          std::string countStr = Trim(inner.substr(0, comma));
          std::string listStr = inner.substr(comma + 1);
          int count = 0;
          bool validCount = false;
          try {
            size_t pos;
            count = std::stoi(countStr, &pos);
            validCount = pos == countStr.size() && count > 0;
          } catch (const std::exception &) {
            // Not a bare integer (e.g. "auto-fill"/"auto-fit") - unsupported.
          }
          std::string lowerList = ToLower(listStr);
          if (validCount && lowerList.find("repeat(") == std::string::npos &&
              lowerList.find("minmax(") == std::string::npos) {
            std::vector<GridTrack> innerTracks;
            std::istringstream listTokens(listStr);
            std::string tok;
            while (listTokens >> tok) {
              ParseGridTrackToken(tok, innerTracks);
            }
            for (int n = 0; n < count; n++) {
              tracks.insert(tracks.end(), innerTracks.begin(), innerTracks.end());
            }
          }
        }
        i = j;
        continue;
      }
      // Unbalanced parens - fall through and consume it as a regular
      // (invalid, so skipped) whitespace-delimited token below.
    }
    if (raw.size() - i >= 7 && ToLower(raw.substr(i, 7)) == "minmax(") {
      size_t j = ScanBalancedParens(raw, i + 6);
      if (j != std::string::npos) {
        std::string inner = raw.substr(i + 7, (j - 1) - (i + 7));
        GridTrack track;
        if (ParseMinMaxTrack(inner, track)) {
          tracks.push_back(track);
        }
        i = j;
        continue;
      }
      // Unbalanced parens - same fall-through as repeat() above.
    }
    size_t start = i;
    while (i < raw.size() && !std::isspace(static_cast<unsigned char>(raw[i]))) {
      i++;
    }
    ParseGridTrackToken(raw.substr(start, i - start), tracks);
  }
  if (tracks.empty()) {
    return false;
  }
  out = std::move(tracks);
  return true;
}

// Parses grid-template-areas - one or more quoted strings (typically
// one per source line, though this scans for quote characters directly
// rather than splitting on newlines, so it doesn't actually care how
// they're laid out), each becoming one grid row; each whitespace-
// separated token within a string becomes that row's per-column area
// name ("." is a real CSS keyword for "this cell has no name" - kept
// as a literal token here, same as any other name, since
// FindGridAreaPlacement, widget_renderer.cpp, only ever searches for a
// specific name an item's grid-area actually names, never "."
// specifically). Doesn't validate that every row has the same number
// of tokens, or that a name's occurrences form a real CSS's own
// solid-rectangle requirement - RenderGridContainer's own
// FindGridAreaPlacement just takes whichever bounding box a name's
// occurrences span. Returns false (leaving `out` untouched) if no
// quoted string with at least one token was found at all.
bool ParseGridTemplateAreas(const std::string &raw,
                             std::vector<std::vector<std::string>> &out) {
  std::vector<std::vector<std::string>> rows;
  size_t i = 0;
  while (i < raw.size()) {
    while (i < raw.size() && raw[i] != '"' && raw[i] != '\'') {
      ++i;
    }
    if (i >= raw.size()) {
      break;
    }
    char quote = raw[i];
    size_t start = i + 1;
    size_t end = raw.find(quote, start);
    if (end == std::string::npos) {
      break;
    }
    std::istringstream tokens(raw.substr(start, end - start));
    std::string token;
    std::vector<std::string> row;
    while (tokens >> token) {
      row.push_back(token);
    }
    if (!row.empty()) {
      rows.push_back(std::move(row));
    }
    i = end + 1;
  }
  if (rows.empty()) {
    return false;
  }
  out = std::move(rows);
  return true;
}

// One side of a grid-column/grid-row value - either a bare integer
// (a 1-indexed line number) or a "span N" token. Returns false (leaving
// `isSpan`/`value` untouched) for anything else, including a bare
// "span" with no following number.
bool ParseGridLineSide(const std::string &side, bool &isSpan, int &value) {
  if (side.empty()) {
    return false;
  }
  std::string lower = ToLower(side);
  if (lower.rfind("span", 0) == 0) {
    try {
      value = std::stoi(Trim(lower.substr(4)));
      isSpan = true;
      return value >= 1;
    } catch (const std::exception &) {
      return false;
    }
  }
  try {
    value = std::stoi(side);
    isSpan = false;
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

// Parses a grid-column/grid-row value - see GridLinePlacement (widget.h)
// for the forms supported: a bare line number, "span N", "<line> /
// <line>", "<line> / span N", or "span N / <line>" (the last resolves
// to an explicit start by subtracting the span from the end line, since
// GridLinePlacement itself only ever stores a start + span, not two
// independent line numbers). "span N / span N" (two spans, no line at
// all) isn't meaningful in real CSS either - falls back to treating the
// first side as a plain span with no explicit start. A line number may
// be negative - real CSS's own "count backward from the explicit grid's
// last line" form (`-1` is that last line, `-2` the one before it, and
// so on) - stored in GridLinePlacement::start exactly as parsed (still
// negative) and left unresolved until ResolveGridLineStart
// (widget_renderer.cpp) runs, once the explicit track count it's
// relative to is actually known; this function only ever computes a
// *span* from two explicit line numbers (the "<line> / <line>" case
// below), which stays correct un-resolved as long as both sides share
// the same sign (the difference between two negative "virtual" line
// numbers is the same span a real CSS resolver would get) - a mixed
// positive/negative pair is rejected instead of computing a wrong span
// (see below). Real CSS's named lines aren't supported. Returns false
// (leaving `out` untouched) if the left side (or the only side, when
// there's no '/') doesn't parse at all, or if two explicit line numbers
// have mixed signs.
bool ParseGridLinePlacement(const std::string &raw, GridLinePlacement &out) {
  std::string text = Trim(raw);
  size_t slash = text.find('/');
  std::string left = Trim(slash == std::string::npos ? text : text.substr(0, slash));
  std::string right =
      slash == std::string::npos ? std::string() : Trim(text.substr(slash + 1));

  bool leftIsSpan = false;
  int leftValue = 0;
  if (!ParseGridLineSide(left, leftIsSpan, leftValue)) {
    return false;
  }

  bool rightIsSpan = false;
  int rightValue = 0;
  bool haveRight = !right.empty() && ParseGridLineSide(right, rightIsSpan, rightValue);

  if (!haveRight) {
    out = leftIsSpan ? GridLinePlacement{false, 0, std::max(1, leftValue)}
                      : GridLinePlacement{true, leftValue, 1};
    return true;
  }
  if (!leftIsSpan && !rightIsSpan) {
    if ((leftValue < 0) != (rightValue < 0)) {
      // Mixed sign - see this function's own doc comment above for why
      // the span-as-difference math below only stays correct when both
      // explicit line numbers share the same sign.
      return false;
    }
    out = {true, leftValue, std::max(1, rightValue - leftValue)};
  } else if (!leftIsSpan && rightIsSpan) {
    out = {true, leftValue, std::max(1, rightValue)};
  } else if (leftIsSpan && !rightIsSpan) {
    int span = std::max(1, leftValue);
    int computedStart = rightValue - span;
    // The >= 1 floor below only makes sense for a positive end line -
    // for a negative one (`span N / -1`), computedStart needs to stay
    // negative so ResolveGridLineStart (widget_renderer.cpp) can still
    // resolve it relative to the explicit grid's own end, the same
    // deferred-resolution reasoning this function's own doc comment
    // above explains for the plain-negative-start case.
    out = {true, rightValue > 0 ? std::max(1, computedStart) : computedStart, span};
  } else {
    out = {false, 0, std::max(1, leftValue)};
  }
  return true;
}

// Parses real CSS's numeric `grid-area: <row-start> / <column-start> /
// <row-end> / <column-end>` shorthand into the row/column
// GridLinePlacement pair it's equivalent to - each side the same bare
// line number or "span N" grid-column/grid-row's own value already
// accepts (see ParseGridLineSide/ParseGridLinePlacement just above),
// reused per axis here (a "<start> / <end>" spec built from the
// shorthand's own row or column pair, or just "<start>" alone for the
// 2-value form below) rather than duplicating that parsing. Only the
// full 4-value form and the 2-value `row-start / column-start` form
// (each start's own end implicitly span 1, same as ParseGridLinePlacement's
// "no right side" case) are supported; real CSS's 1-value and 3-value
// forms - which infer a missing end from the *start* side, a rule only
// meaningful for named lines (unsupported in this bounded subset
// anyway) - aren't, so a 1- or 3-part value here just returns false,
// falling through to the plain named-area string handling below like
// any other value this doesn't recognize. Named lines aren't supported
// on this path either, same restriction grid-column/grid-row's own
// numeric form already has.
bool ParseGridAreaShorthand(const std::string &raw, GridLinePlacement &row,
                             GridLinePlacement &column) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (true) {
    size_t slash = raw.find('/', start);
    size_t len = slash == std::string::npos ? std::string::npos : slash - start;
    parts.push_back(Trim(raw.substr(start, len)));
    if (slash == std::string::npos) {
      break;
    }
    start = slash + 1;
  }
  if (parts.size() != 2 && parts.size() != 4) {
    return false;
  }
  std::string rowSpec = parts.size() == 4 ? parts[0] + " / " + parts[2] : parts[0];
  std::string columnSpec = parts.size() == 4 ? parts[1] + " / " + parts[3] : parts[1];
  return ParseGridLinePlacement(rowSpec, row) && ParseGridLinePlacement(columnSpec, column);
}

// Parses an nth-*() argument - a literal integer, or the "even"/"odd"
// keywords - into (nthA, nthB), the same v1 grammar nth-child and
// nth-of-type both share (not the full "an+b" algebra). Returns false
// (leaving *outA/*outB untouched) for anything else.
bool ParseNthArg(const std::string &arg, int *outA, int *outB) {
  if (arg == "even") {
    *outA = 2;
    *outB = 0;
    return true;
  }
  if (arg == "odd") {
    *outA = 2;
    *outB = 1;
    return true;
  }
  try {
    *outA = 0;
    *outB = std::stoi(arg);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

// Parses a pseudo-class token (the text after ':', e.g. "first-child",
// "nth-child(2)", "nth-child(even)", "nth-of-type(3)", "hover", "focus",
// "focus-within") and appends it to `selector` - silently ignored
// (matching this file's "skip what we don't understand" philosophy
// elsewhere, e.g. ParseDeclarations) if it isn't one of the pseudo-classes
// this bounded grammar supports.
void ParsePseudoClassToken(const std::string &token, CompoundSelector &selector) {
  std::string lower = ToLower(token);
  // `::before`/`::after` tokenize identically to a single-colon
  // `:before`/`:after` by the time either reaches here - the first `:`
  // of a `::` pair produces its own, separate empty token that
  // ParseCompoundSelector's caller already discards before ever calling
  // this function (see its own comment) - so this one check handles
  // both forms. Unlike everything else in this function, this sets
  // CompoundSelector::pseudoElement, not a PseudoClassSelector - see
  // PseudoElementKind (css.h) for why that's a different kind of thing.
  if (lower == "before") {
    selector.pseudoElement = PseudoElementKind::kBefore;
    return;
  }
  if (lower == "after") {
    selector.pseudoElement = PseudoElementKind::kAfter;
    return;
  }
  PseudoClassSelector pc;
  if (lower == "first-child") {
    pc.kind = PseudoClassKind::kFirstChild;
  } else if (lower == "last-child") {
    pc.kind = PseudoClassKind::kLastChild;
  } else if (lower == "hover") {
    pc.kind = PseudoClassKind::kHover;
  } else if (lower == "focus") {
    pc.kind = PseudoClassKind::kFocus;
  } else if (lower == "focus-within") {
    pc.kind = PseudoClassKind::kFocusWithin;
  } else if (lower.rfind("nth-child(", 0) == 0 && !lower.empty() &&
             lower.back() == ')') {
    std::string arg = Trim(lower.substr(10, lower.size() - 10 - 1));
    pc.kind = PseudoClassKind::kNthChild;
    if (!ParseNthArg(arg, &pc.nthA, &pc.nthB)) {
      return;
    }
  } else if (lower.rfind("nth-of-type(", 0) == 0 && !lower.empty() &&
             lower.back() == ')') {
    std::string arg = Trim(lower.substr(12, lower.size() - 12 - 1));
    pc.kind = PseudoClassKind::kNthOfType;
    if (!ParseNthArg(arg, &pc.nthA, &pc.nthB)) {
      return;
    }
  } else {
    return;
  }
  selector.pseudoClasses.push_back(pc);
}

// Forward-declared so ParseCompoundSelector's :not(...) handling can
// call it - ParseSelectorList itself (defined below) is built out of
// ParseSelectorChain, which is in turn built out of
// ParseCompoundSelector, so this is the same "the two need each other"
// shape MatchesPseudoClass/MatchesChain already have on the matching
// side.
std::vector<Selector> ParseSelectorList(const std::string &raw);

// Splits a compound selector ("div.card#hero", ".a.b", "*", "h1",
// "a[href]", "li:nth-child(2)", "p:not(.intro)") into its tag/id/class/
// attribute/pseudo-class parts. The leading segment (before the first
// special marker) is the tag - absent or "*" means "any tag" (universal).
// Attribute/pseudo-class values are assumed free of whitespace and of
// the marker characters ('.', '#', '[', ':') themselves - same
// bounded-subset philosophy as ParseColor's rgb() parsing elsewhere in
// this file, not a full CSS tokenizer (":not(...)" gets a specific
// exception to that below, since its argument routinely contains those
// marker characters). Used both by StyleSheet::Parse's rule parsing and
// by ParseSelectorChain below (same grammar, different direction - see
// css.h). :not(...)'s own argument is parsed via ParseSelectorList - the
// same comma-separated-list-of-full-chains grammar a top-level selector
// gets, real CSS Level 4 semantics (":not(div > p)" and ":not(a, b)"
// both parse and match the way you'd expect).
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
        // The operator character (~^$*|), if any, sits directly before
        // '=' - not itself part of the name. Plain "[name=value]" has
        // none, leaving nameEnd at eq and op at its default (kEquals).
        size_t nameEnd = eq;
        if (eq > 0) {
          switch (inner[eq - 1]) {
          case '~':
            attr.op = AttributeOperator::kIncludes;
            nameEnd = eq - 1;
            break;
          case '^':
            attr.op = AttributeOperator::kPrefix;
            nameEnd = eq - 1;
            break;
          case '$':
            attr.op = AttributeOperator::kSuffix;
            nameEnd = eq - 1;
            break;
          case '*':
            attr.op = AttributeOperator::kSubstring;
            nameEnd = eq - 1;
            break;
          case '|':
            attr.op = AttributeOperator::kDashMatch;
            nameEnd = eq - 1;
            break;
          default:
            break;
          }
        }
        attr.name = ToLower(Trim(inner.substr(0, nameEnd)));
        std::string attrValue = Trim(inner.substr(eq + 1));
        // The case-insensitivity flag - a trailing, whitespace-separated
        // "i"/"I" *outside* any quotes ("[attr=value i]", not
        // "[attr=\"value i\"]") - checked before quote-stripping below,
        // since a quoted value ending in "i" (e.g. "[attr=\"hi\"]") still
        // ends in a quote character at this point, not 'i'/'I', so it
        // can never be mistaken for the flag.
        if (attrValue.size() >= 2 &&
            (attrValue.back() == 'i' || attrValue.back() == 'I') &&
            std::isspace(static_cast<unsigned char>(attrValue[attrValue.size() - 2]))) {
          attr.caseInsensitive = true;
          attrValue = Trim(attrValue.substr(0, attrValue.size() - 1));
        }
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

    if (marker == ':' && ToLower(value.substr(pos + 1, 4)) == "not(") {
      // :not(...) - find the matching ')' by paren depth (correctly
      // handles nested parens from anything - a nested :not(:not(...)),
      // an :nth-child(...) inside the argument, ...), then parse
      // whatever's between "not(" and it as a full comma-separated
      // selector list (see PseudoClassSelector::notArgs, css.h) rather
      // than falling through to the generic marker-splitting below -
      // which would otherwise mis-terminate at the first '.'/'#'/'['
      // *inside* the argument (e.g. ":not(.card)" would get cut at the
      // '.' in ".card").
      size_t argStart = pos + 1 + 4;
      size_t i = argStart;
      int depth = 1;
      while (i < value.size() && depth > 0) {
        if (value[i] == '(') {
          ++depth;
        } else if (value[i] == ')') {
          --depth;
          if (depth == 0) {
            break;
          }
        }
        ++i;
      }
      PseudoClassSelector pc;
      pc.kind = PseudoClassKind::kNot;
      pc.notArgs = std::make_shared<std::vector<Selector>>(
          ParseSelectorList(value.substr(argStart, i - argStart)));
      selector.pseudoClasses.push_back(std::move(pc));
      pos = (i < value.size()) ? value.find_first_of(".#[:", i + 1)
                                : std::string::npos;
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
// doesn't otherwise support a quoted value containing a space), and
// neither do they inside an unclosed '(' ... ')' - :not()'s own argument
// (see ParseCompoundSelector) can itself be a full chain with
// combinators/spaces of its own ("div:not(div > p)"), which must stay
// part of the *outer* selector's single compound token, not get read as
// more of the outer chain.
Selector ParseSelectorChain(const std::string &raw) {
  Selector selector;
  std::string text = Trim(raw);
  std::string currentToken;
  bool pendingCombinator = false;
  Combinator combinator = Combinator::kDescendant;
  int bracketDepth = 0;
  int parenDepth = 0;

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
    if (c == '(') {
      ++parenDepth;
      currentToken += c;
      continue;
    }
    if (c == ')') {
      if (parenDepth > 0) {
        --parenDepth;
      }
      currentToken += c;
      continue;
    }
    if (bracketDepth == 0 && parenDepth == 0 && (c == '>' || c == '+' || c == '~')) {
      flushToken();
      combinator = (c == '>')   ? Combinator::kChild
                   : (c == '+') ? Combinator::kAdjacentSibling
                                : Combinator::kGeneralSibling;
      pendingCombinator = true;
      continue;
    }
    if (bracketDepth == 0 && parenDepth == 0 &&
        std::isspace(static_cast<unsigned char>(c))) {
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
// cascade for a given element), QuerySelector/QuerySelectorAll/
// ElementMatches/Closest below (ANY of which counts as a match), and
// ParseCompoundSelector's own :not(...) handling (:not()'s argument is
// itself a comma-separated list - see PseudoClassSelector::notArgs,
// css.h). A comma inside an unclosed '(' ... ')' or '[' ... ']' doesn't
// split the list - "div:not(a, b), span" is two alternatives (the first
// itself containing a nested, un-split list inside :not()), not four,
// and "[title=\"a,b\"]" isn't torn in two either, mirroring
// ParseSelectorChain's identical bracket/paren-awareness for
// combinators/whitespace. Discards a selector that parsed to zero
// compounds (empty/malformed input) rather than letting it match
// everything.
std::vector<Selector> ParseSelectorList(const std::string &raw) {
  std::vector<Selector> result;
  auto flushPart = [&](const std::string &part) {
    std::string trimmed = Trim(part);
    if (trimmed.empty()) {
      return;
    }
    Selector selector = ParseSelectorChain(trimmed);
    if (!selector.compounds.empty()) {
      result.push_back(std::move(selector));
    }
  };

  std::string part;
  int depth = 0;
  for (char c : raw) {
    if (c == '(' || c == '[') {
      ++depth;
      part += c;
    } else if (c == ')' || c == ']') {
      if (depth > 0) {
        --depth;
      }
      part += c;
    } else if (c == ',' && depth == 0) {
      flushPart(part);
      part.clear();
    } else {
      part += c;
    }
  }
  flushPart(part);
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
      } else if (lower == "grid") {
        decl.hasDisplay = true;
        decl.display = DisplayMode::kGrid;
      } else if (lower == "block") {
        decl.hasDisplay = true;
        decl.display = DisplayMode::kBlock;
      }
    } else if (property == "grid-template-columns") {
      // `subgrid` replaces the whole track list, not a token within it
      // (real CSS's own grammar keeps it mutually exclusive with a
      // literal track list too) - columns only, see
      // Widget::gridTemplateColumnsSubgrid (widget.h) for why
      // grid-template-rows: subgrid isn't recognized at all here (falls
      // through to ParseGridTrackList below, which - like any other
      // unrecognized token - just finds no valid track in "subgrid" and
      // leaves the property unset).
      if (ToLower(Trim(value)) == "subgrid") {
        decl.hasGridTemplateColumns = true;
        decl.gridTemplateColumns.clear();
        decl.gridTemplateColumnsSubgrid = true;
      } else {
        decl.hasGridTemplateColumns =
            ParseGridTrackList(value, decl.gridTemplateColumns);
        decl.gridTemplateColumnsSubgrid = false;
      }
    } else if (property == "grid-template-rows") {
      decl.hasGridTemplateRows = ParseGridTrackList(value, decl.gridTemplateRows);
    } else if (property == "grid-template-areas") {
      decl.hasGridTemplateAreas =
          ParseGridTemplateAreas(value, decl.gridTemplateAreas);
    } else if (property == "grid-area") {
      // The numeric line-based shorthand (see ParseGridAreaShorthand
      // above) expands directly into grid-row/grid-column - the exact
      // same fields (and cascade PropertyWinner) a literal grid-row/
      // grid-column declaration on this same selector would use, which
      // is what makes this a real shorthand rather than a separate
      // property. decl.gridArea itself is left empty in that case, so
      // FindGridAreaPlacement's named-area lookup (widget_renderer.cpp)
      // naturally never matches it - only a value that doesn't parse as
      // the numeric form falls back to being treated as a plain area
      // name instead.
      GridLinePlacement row, column;
      if (ParseGridAreaShorthand(value, row, column)) {
        decl.hasGridRow = true;
        decl.gridRow = row;
        decl.hasGridColumn = true;
        decl.gridColumn = column;
      } else {
        decl.gridArea = value;
      }
    } else if (property == "grid-column") {
      decl.hasGridColumn = ParseGridLinePlacement(value, decl.gridColumn);
    } else if (property == "grid-row") {
      decl.hasGridRow = ParseGridLinePlacement(value, decl.gridRow);
    } else if (property == "grid-auto-flow") {
      // Real CSS's valid values are `row`, `column`, `row dense`,
      // `column dense`, and bare `dense` (implying row) - only the
      // leading row/column keyword is ever consulted here, since this
      // engine's auto-placement has never tracked cell occupancy at all
      // (see GridAutoFlow, widget.h) and `dense` packing is meaningless
      // without it, but a `dense` modifier still parses (rather than
      // making the whole property unrecognized) so it doesn't lose
      // whichever row/column keyword came with it.
      std::string lower = ToLower(value);
      std::istringstream tokens(lower);
      std::string first;
      tokens >> first;
      if (first == "row" || first == "dense") {
        decl.hasGridAutoFlow = true;
        decl.gridAutoFlow = GridAutoFlow::kRow;
      } else if (first == "column") {
        decl.hasGridAutoFlow = true;
        decl.gridAutoFlow = GridAutoFlow::kColumn;
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
      // flex-start/flex-end and start/end are accepted as the same pair
      // of spellings align-items already does just below, for the same
      // reason (no writing-mode distinction here for either flexbox or
      // grid, so they're behaviorally identical regardless of which
      // layout mode ends up active).
      std::string lower = ToLower(value);
      if (lower == "flex-start" || lower == "start") {
        decl.hasJustifyContent = true;
        decl.justifyContent = JustifyContent::kFlexStart;
      } else if (lower == "center") {
        decl.hasJustifyContent = true;
        decl.justifyContent = JustifyContent::kCenter;
      } else if (lower == "flex-end" || lower == "end") {
        decl.hasJustifyContent = true;
        decl.justifyContent = JustifyContent::kFlexEnd;
      } else if (lower == "space-between") {
        decl.hasJustifyContent = true;
        decl.justifyContent = JustifyContent::kSpaceBetween;
      } else if (lower == "space-around") {
        decl.hasJustifyContent = true;
        decl.justifyContent = JustifyContent::kSpaceAround;
      } else if (lower == "space-evenly") {
        decl.hasJustifyContent = true;
        decl.justifyContent = JustifyContent::kSpaceEvenly;
      }
    } else if (property == "align-items") {
      // Genuinely the same property for both flexbox and CSS Grid in
      // real CSS (interpreted differently by whichever layout algorithm
      // is active - RenderFlexContainer/RenderGridContainer,
      // widget_renderer.cpp - since a container is always exactly one
      // or the other), so this parses both flexbox's own `flex-start`/
      // `flex-end` keyword spelling and grid's `start`/`end` one into
      // the same AlignItems value - real CSS only accepts the *grid*
      // spelling on a grid container and the *flex* one on a flex
      // container, but this parser has no way to know which context a
      // rule will end up matching yet, so it accepts either everywhere.
      std::string lower = ToLower(value);
      if (lower == "flex-start" || lower == "start") {
        decl.hasAlignItems = true;
        decl.alignItems = AlignItems::kFlexStart;
      } else if (lower == "center") {
        decl.hasAlignItems = true;
        decl.alignItems = AlignItems::kCenter;
      } else if (lower == "flex-end" || lower == "end") {
        decl.hasAlignItems = true;
        decl.alignItems = AlignItems::kFlexEnd;
      } else if (lower == "stretch") {
        decl.hasAlignItems = true;
        decl.alignItems = AlignItems::kStretch;
      }
    } else if (property == "justify-items") {
      // Grid only - flexbox has no equivalent property (justify-content
      // is flex's own inline-axis alignment, but it distributes free
      // space among *items*, not each item within its own line the way
      // this positions an item within its own cell - a different
      // concept entirely, hence its own AlignItems-typed field
      // (Declarations::justifyItems) rather than reusing
      // JustifyContent). Same four keywords grid's own align-items
      // above accepts.
      std::string lower = ToLower(value);
      if (lower == "start") {
        decl.hasJustifyItems = true;
        decl.justifyItems = AlignItems::kFlexStart;
      } else if (lower == "center") {
        decl.hasJustifyItems = true;
        decl.justifyItems = AlignItems::kCenter;
      } else if (lower == "end") {
        decl.hasJustifyItems = true;
        decl.justifyItems = AlignItems::kFlexEnd;
      } else if (lower == "stretch") {
        decl.hasJustifyItems = true;
        decl.justifyItems = AlignItems::kStretch;
      }
    } else if (property == "justify-self") {
      // Grid-only item property, same four keywords justify-items
      // above accepts (this parser never wires justify-self into
      // flexbox, so there's no flex-start/flex-end spelling to accept
      // here the way align-items/align-self do).
      std::string lower = ToLower(value);
      if (lower == "start") {
        decl.hasJustifySelf = true;
        decl.justifySelf = AlignItems::kFlexStart;
      } else if (lower == "center") {
        decl.hasJustifySelf = true;
        decl.justifySelf = AlignItems::kCenter;
      } else if (lower == "end") {
        decl.hasJustifySelf = true;
        decl.justifySelf = AlignItems::kFlexEnd;
      } else if (lower == "stretch") {
        decl.hasJustifySelf = true;
        decl.justifySelf = AlignItems::kStretch;
      }
    } else if (property == "align-self") {
      // Grid-only item property here (real CSS's align-self also
      // applies to flex items, but RenderFlexContainer doesn't read
      // this field - see Widget::alignSelf, widget.h) - same four
      // keywords align-items above accepts, grid's own start/end
      // spelling only.
      std::string lower = ToLower(value);
      if (lower == "start") {
        decl.hasAlignSelf = true;
        decl.alignSelf = AlignItems::kFlexStart;
      } else if (lower == "center") {
        decl.hasAlignSelf = true;
        decl.alignSelf = AlignItems::kCenter;
      } else if (lower == "end") {
        decl.hasAlignSelf = true;
        decl.alignSelf = AlignItems::kFlexEnd;
      } else if (lower == "stretch") {
        decl.hasAlignSelf = true;
        decl.alignSelf = AlignItems::kStretch;
      }
    } else if (property == "align-content") {
      // flex-start/flex-end and start/end - same accepted-spelling pair
      // justify-content above and align-items just above that already
      // have, for the same reason.
      std::string lower = ToLower(value);
      if (lower == "flex-start" || lower == "start") {
        decl.hasAlignContent = true;
        decl.alignContent = AlignContent::kFlexStart;
      } else if (lower == "center") {
        decl.hasAlignContent = true;
        decl.alignContent = AlignContent::kCenter;
      } else if (lower == "flex-end" || lower == "end") {
        decl.hasAlignContent = true;
        decl.alignContent = AlignContent::kFlexEnd;
      } else if (lower == "space-between") {
        decl.hasAlignContent = true;
        decl.alignContent = AlignContent::kSpaceBetween;
      } else if (lower == "space-around") {
        decl.hasAlignContent = true;
        decl.alignContent = AlignContent::kSpaceAround;
      } else if (lower == "space-evenly") {
        decl.hasAlignContent = true;
        decl.alignContent = AlignContent::kSpaceEvenly;
      } else if (lower == "stretch") {
        decl.hasAlignContent = true;
        decl.alignContent = AlignContent::kStretch;
      }
    } else if (property == "gap") {
      decl.hasGap = ParsePixelLength(value, decl.gap);
    } else if (property == "column-gap") {
      decl.hasColumnGap = ParsePixelLength(value, decl.columnGap);
    } else if (property == "row-gap") {
      decl.hasRowGap = ParsePixelLength(value, decl.rowGap);
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
    } else if (property == "content") {
      // Only a plain quoted string or `none` - real CSS's attr()/
      // counter()/open-quote/close-quote and any other dynamic content
      // form are all unsupported, same bounded-subset philosophy as the
      // rest of this parser (see Declarations::content, css.h, for why
      // `none` still sets hasContent rather than leaving it unset).
      std::string trimmed = Trim(value);
      if (ToLower(trimmed) == "none") {
        decl.hasContent = true;
        decl.content.clear();
      } else if (trimmed.size() >= 2 &&
                 (trimmed.front() == '"' || trimmed.front() == '\'') &&
                 trimmed.back() == trimmed.front()) {
        decl.hasContent = true;
        decl.content = trimmed.substr(1, trimmed.size() - 2);
      }
    }
    // Any other property is silently ignored, same as a browser skipping
    // one it doesn't recognize.
  }

  return decl;
}

// Whether `index` (1-based) satisfies nth-child/nth-of-type's "an+b"
// v1 grammar (a literal index, or the even/odd keywords - see
// ParseNthArg). nthA == 0 means a literal index (match that one
// position only); otherwise nthA is always 2 (even/odd) - both
// non-negative, so plain modulo is safe here (no negative-operand sign
// concerns a general "an+b" would have to worry about).
bool MatchesNth(const PseudoClassSelector &pc, int index) {
  return pc.nthA == 0 ? index == pc.nthB : (index % pc.nthA) == pc.nthB;
}

// Whether `node` is `from` itself, or one of `from`'s ancestors -
// shared by :hover (from = the hovered node) and :focus-within
// (from = the focused node), which both bubble to every containing
// element the same way. `from` is nullptr when nothing is
// hovered/focused, in which case nothing ever matches.
bool MatchesSelfOrAncestor(const Node *from, const Node &node) {
  for (const Node *n = from; n != nullptr; n = n->parent()) {
    if (n == &node) {
      return true;
    }
  }
  return false;
}

// Forward-declared so MatchesPseudoClass's kNot case can call back into
// it - CompoundMatches (below) is what actually invokes
// MatchesPseudoClass for each of a compound's pseudo-classes, and
// MatchesChain (further below, built out of CompoundMatches) is what
// kNot itself needs - full selector-chain matching, combinators and
// all, for each alternative in notArgs. All mutually recursive
// (":not(div > p:nth-child(2))" needs every layer of it).
bool MatchesChain(const Selector &selector, const Node &node,
                   const PseudoClassState &state);

// Whether `node` is at the position `pc` describes among its parent's
// *element* siblings (1-based - real CSS's :nth-child counts only
// element children). A node with no parent (root/detached) is treated
// as its own only sibling, so :first-child/:last-child/:nth-child(1)
// all vacuously match it - same as real CSS resolves an unparented
// element. `state` supplies what :hover/:focus need (see
// PseudoClassState, css.h) - the other kinds ignore it entirely.
bool MatchesPseudoClass(const PseudoClassSelector &pc, const Node &node,
                         const PseudoClassState &state) {
  if (pc.kind == PseudoClassKind::kNot) {
    // notArgs is null, or empty, only when `:not()` had no parseable
    // argument (e.g. ":not()" itself, or a comma list where every part
    // was empty/malformed) - treat that degenerate case as never
    // negating anything, i.e. :not() with no real constraint matches
    // like a bare "*" would. Otherwise: matches unless `node` matches
    // *any* of the alternatives - full chain matching, combinators and
    // all, not just a bare compound (real CSS Level 4's :not(list)
    // semantics - see ParseSelectorList/PseudoClassSelector::notArgs).
    if (pc.notArgs == nullptr || pc.notArgs->empty()) {
      return true;
    }
    for (const Selector &alternative : *pc.notArgs) {
      if (MatchesChain(alternative, node, state)) {
        return false;
      }
    }
    return true;
  }
  if (pc.kind == PseudoClassKind::kHover) {
    // :hover matches `node` itself and every one of its ancestors -
    // hovering a child counts as hovering its containers too, same as
    // real CSS.
    return MatchesSelfOrAncestor(state.hovered, node);
  }
  if (pc.kind == PseudoClassKind::kFocus) {
    // Unlike :hover, real CSS doesn't bubble plain :focus to ancestors -
    // only the exact focused node matches (see kFocusWithin below for
    // the pseudo-class that does bubble).
    return state.focused == &node;
  }
  if (pc.kind == PseudoClassKind::kFocusWithin) {
    // Same walk :hover uses, just off state.focused instead of
    // state.hovered - matches `node` and every one of its ancestors, so
    // a form wrapper can style itself while any field inside it has
    // focus.
    return MatchesSelfOrAncestor(state.focused, node);
  }
  if (pc.kind == PseudoClassKind::kNthOfType) {
    // Same shape as the kFirstChild/kLastChild/kNthChild counting below,
    // but counting only same-tag siblings - `p:nth-of-type(2)` is the
    // second <p> among its siblings, not the second element overall,
    // real CSS's actual definition (unlike :nth-child, there's no
    // separate :first-of-type/:last-of-type here, just this one).
    const Node *parent = node.parent();
    int index = 1;
    if (parent != nullptr) {
      index = 0;
      int typeCount = 0;
      for (const auto &siblingPtr : parent->children()) {
        const Node *sibling = siblingPtr.get();
        if (sibling->type() != NodeType::kElement ||
            sibling->tagName() != node.tagName()) {
          continue;
        }
        ++typeCount;
        if (sibling == &node) {
          index = typeCount;
        }
      }
      if (index == 0) {
        return false;
      }
    }
    return MatchesNth(pc, index);
  }

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
    return MatchesNth(pc, index);
  case PseudoClassKind::kNthOfType:
  case PseudoClassKind::kHover:
  case PseudoClassKind::kFocus:
  case PseudoClassKind::kFocusWithin:
  case PseudoClassKind::kNot:
    // Handled by the early returns above - unreachable here, but listed
    // so this switch stays exhaustive (no -Wswitch warning) as
    // PseudoClassKind grows.
    return false;
  }
  return false;
}

// Whether `rawValue` (an attribute's actual current text) satisfies
// `attr`'s operator against `attr.value` - only called once both are
// known to exist (a null/absent attribute, or an attr.value-less
// "[name]", never reach here; see CompoundMatches below). An empty
// `*attr.value` never matches kPrefix/kSuffix/kSubstring, matching real
// CSS (`[href^=""]` selects nothing, not everything). `attr.caseInsensitive`
// (the trailing `i` flag - "[attr=value i]") lowercases both sides first,
// same ASCII-only case folding this file's other lowercasing (tag names,
// pseudo-class keywords, ...) already uses - not full Unicode case
// folding, matching this file's usual bounded-subset approach.
bool AttributeValueMatches(const std::string &rawValue, const AttributeSelector &attr) {
  std::string value = attr.caseInsensitive ? ToLower(rawValue) : rawValue;
  std::string want = attr.caseInsensitive ? ToLower(*attr.value) : *attr.value;
  switch (attr.op) {
  case AttributeOperator::kEquals:
    return value == want;
  case AttributeOperator::kIncludes: {
    std::stringstream ss(value);
    std::string token;
    while (ss >> token) {
      if (token == want) {
        return true;
      }
    }
    return false;
  }
  case AttributeOperator::kPrefix:
    return !want.empty() && value.compare(0, want.size(), want) == 0;
  case AttributeOperator::kSuffix:
    return !want.empty() && value.size() >= want.size() &&
           value.compare(value.size() - want.size(), want.size(), want) == 0;
  case AttributeOperator::kSubstring:
    return !want.empty() && value.find(want) != std::string::npos;
  case AttributeOperator::kDashMatch:
    // Exactly `want`, or `want` followed immediately by a hyphen -
    // "en" matches "en"/"en-US" but not "english"/"en2". Unlike
    // ^=/$=/*= above, an empty `want` isn't special-cased to never
    // match: by this same rule, it matches an empty value or any value
    // starting with "-", which is a real (if unusual) answer, not a
    // vacuous "matches everything".
    return value == want || (value.size() > want.size() &&
                              value.compare(0, want.size(), want) == 0 &&
                              value[want.size()] == '-');
  }
  return false;
}

// Every part of a compound selector must match - a tag constraint, an id
// constraint, every listed class, every attribute constraint, and every
// pseudo-class, all AND'd together (an empty part imposes no constraint,
// so a bare CompoundSelector{} - "*" - matches anything). The rightmost
// compound of a Selector chain - see MatchesChain below for the rest of
// the chain (ancestor/sibling compounds).
bool CompoundMatches(const CompoundSelector &selector, const Node &node,
                      const PseudoClassState &state) {
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
    if (attr.value.has_value() && !AttributeValueMatches(*value, attr)) {
      return false;
    }
  }

  for (const PseudoClassSelector &pc : selector.pseudoClasses) {
    if (!MatchesPseudoClass(pc, node, state)) {
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
                           const Node *contextNode,
                           const PseudoClassState &state) {
  if (upToIndex < 0) {
    return true;
  }

  const CompoundSelector &compound = selector.compounds[upToIndex];
  switch (selector.combinators[upToIndex]) {
  case Combinator::kDescendant:
    for (const Node *ancestor = contextNode->parent(); ancestor != nullptr;
         ancestor = ancestor->parent()) {
      if (CompoundMatches(compound, *ancestor, state) &&
          MatchesAncestorChain(selector, upToIndex - 1, ancestor, state)) {
        return true;
      }
    }
    return false;
  case Combinator::kChild: {
    const Node *parent = contextNode->parent();
    return parent != nullptr && CompoundMatches(compound, *parent, state) &&
           MatchesAncestorChain(selector, upToIndex - 1, parent, state);
  }
  case Combinator::kAdjacentSibling: {
    const Node *prev = contextNode->previousSibling();
    return prev != nullptr && CompoundMatches(compound, *prev, state) &&
           MatchesAncestorChain(selector, upToIndex - 1, prev, state);
  }
  case Combinator::kGeneralSibling:
    for (const Node *prev = contextNode->previousSibling(); prev != nullptr;
         prev = prev->previousSibling()) {
      if (CompoundMatches(compound, *prev, state) &&
          MatchesAncestorChain(selector, upToIndex - 1, prev, state)) {
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
bool MatchesChain(const Selector &selector, const Node &node,
                   const PseudoClassState &state) {
  if (selector.compounds.empty()) {
    return false;
  }
  if (!CompoundMatches(selector.compounds.back(), node, state)) {
    return false;
  }
  return MatchesAncestorChain(selector, static_cast<int>(selector.compounds.size()) - 2,
                               &node, state);
}

// Forward-declared so SpecificityOfCompound's kNot case can call it -
// Specificity (below) is built out of SpecificityOfCompound, so the two
// need each other the same way MatchesPseudoClass/MatchesChain already
// do on the matching side.
int Specificity(const Selector &selector);

int SpecificityOfCompound(const CompoundSelector &compound) {
  int score = 0;
  if (!compound.id.empty()) {
    score += 100;
  }
  score += static_cast<int>(compound.classes.size()) * 10;
  score += static_cast<int>(compound.attributes.size()) * 10;
  // Real CSS treats :not(...) as contributing whichever of its
  // argument's alternatives is *most* specific, not a flat pseudo-class
  // point - :not(#foo) counts as an id (100), :not(div) as a tag (1),
  // :not(#foo, div) as an id (100, the larger of the two) - matching
  // what :not() actually does semantically (it's the thing inside that
  // determines how "specific" the constraint is). Every other
  // pseudo-class here is a flat 10, same as a class or attribute
  // selector.
  for (const PseudoClassSelector &pc : compound.pseudoClasses) {
    if (pc.kind == PseudoClassKind::kNot && pc.notArgs != nullptr) {
      int best = 0;
      for (const Selector &alternative : *pc.notArgs) {
        best = std::max(best, Specificity(alternative));
      }
      score += best;
    } else {
      score += 10;
    }
  }
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
                   const PseudoClassState &state, const Callback &callback) {
  for (const auto &childPtr : root.children()) {
    Node *child = childPtr.get();
    bool matched = false;
    for (const Selector &selector : selectors) {
      if (MatchesChain(selector, *child, state)) {
        matched = true;
        break;
      }
    }
    if (matched) {
      if (!callback(child)) {
        return;
      }
    }
    ForEachMatch(selectors, *child, state, callback);
  }
}
} // namespace

Node *QuerySelector(Node &root, const std::string &selector,
                     const PseudoClassState &pseudoState) {
  std::vector<Selector> parsed = ParseSelectorList(selector);
  Node *found = nullptr;
  ForEachMatch(parsed, root, pseudoState, [&](Node *match) {
    found = match;
    return false; // Stop at the first match.
  });
  return found;
}

std::vector<Node *> QuerySelectorAll(Node &root, const std::string &selector,
                                      const PseudoClassState &pseudoState) {
  std::vector<Selector> parsed = ParseSelectorList(selector);
  std::vector<Node *> results;
  ForEachMatch(parsed, root, pseudoState, [&](Node *match) {
    results.push_back(match);
    return true; // Keep going - collect every match.
  });
  return results;
}

bool ElementMatches(const Node &node, const std::string &selector,
                     const PseudoClassState &pseudoState) {
  for (const Selector &parsed : ParseSelectorList(selector)) {
    if (MatchesChain(parsed, node, pseudoState)) {
      return true;
    }
  }
  return false;
}

Node *Closest(Node &node, const std::string &selector,
              const PseudoClassState &pseudoState) {
  std::vector<Selector> parsed = ParseSelectorList(selector);
  for (Node *n = &node; n != nullptr; n = n->parent()) {
    for (const Selector &selector : parsed) {
      if (MatchesChain(selector, *n, pseudoState)) {
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
  if (inlineStyle.hasJustifyItems) {
    declarations.hasJustifyItems = true;
    declarations.justifyItems = inlineStyle.justifyItems;
  }
  if (inlineStyle.hasAlignContent) {
    declarations.hasAlignContent = true;
    declarations.alignContent = inlineStyle.alignContent;
  }
  if (inlineStyle.hasGap) {
    declarations.hasGap = true;
    declarations.gap = inlineStyle.gap;
  }
  if (inlineStyle.hasColumnGap) {
    declarations.hasColumnGap = true;
    declarations.columnGap = inlineStyle.columnGap;
  }
  if (inlineStyle.hasRowGap) {
    declarations.hasRowGap = true;
    declarations.rowGap = inlineStyle.rowGap;
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
  if (inlineStyle.hasGridTemplateColumns) {
    declarations.hasGridTemplateColumns = true;
    declarations.gridTemplateColumns = inlineStyle.gridTemplateColumns;
    declarations.gridTemplateColumnsSubgrid = inlineStyle.gridTemplateColumnsSubgrid;
  }
  if (inlineStyle.hasGridTemplateRows) {
    declarations.hasGridTemplateRows = true;
    declarations.gridTemplateRows = inlineStyle.gridTemplateRows;
  }
  if (inlineStyle.hasGridTemplateAreas) {
    declarations.hasGridTemplateAreas = true;
    declarations.gridTemplateAreas = inlineStyle.gridTemplateAreas;
  }
  if (!inlineStyle.gridArea.empty()) {
    declarations.gridArea = inlineStyle.gridArea;
  }
  if (inlineStyle.hasGridColumn) {
    declarations.hasGridColumn = true;
    declarations.gridColumn = inlineStyle.gridColumn;
  }
  if (inlineStyle.hasGridRow) {
    declarations.hasGridRow = true;
    declarations.gridRow = inlineStyle.gridRow;
  }
  if (inlineStyle.hasJustifySelf) {
    declarations.hasJustifySelf = true;
    declarations.justifySelf = inlineStyle.justifySelf;
  }
  if (inlineStyle.hasAlignSelf) {
    declarations.hasAlignSelf = true;
    declarations.alignSelf = inlineStyle.alignSelf;
  }
  if (inlineStyle.hasGridAutoFlow) {
    declarations.hasGridAutoFlow = true;
    declarations.gridAutoFlow = inlineStyle.gridAutoFlow;
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

Declarations StyleSheet::Resolve(const Node &node, const Declarations &inherited,
                                  const PseudoClassState &pseudoState,
                                  PseudoElementKind target) const {
  Declarations own;

  PropertyWinner colorWin, boldWin, bgWin, borderColorWin, borderWidthWin;
  PropertyWinner contentWin;
  PropertyWinner widthWin, heightWin;
  PropertyWinner paddingTopWin, paddingRightWin, paddingBottomWin, paddingLeftWin;
  PropertyWinner marginTopWin, marginRightWin, marginBottomWin, marginLeftWin;
  PropertyWinner displayWin, flexDirectionWin, justifyContentWin, alignItemsWin, gapWin;
  PropertyWinner columnGapWin, rowGapWin;
  PropertyWinner justifyItemsWin;
  PropertyWinner alignContentWin;
  PropertyWinner flexWrapWin, flexGrowWin, flexShrinkWin, flexBasisWin;
  PropertyWinner gridTemplateColumnsWin, gridTemplateRowsWin, gridTemplateAreasWin;
  PropertyWinner gridAreaWin;
  PropertyWinner gridColumnWin, gridRowWin;
  PropertyWinner justifySelfWin, alignSelfWin;
  PropertyWinner gridAutoFlowWin;

  for (size_t i = 0; i < rules_.size(); ++i) {
    const Rule &rule = rules_[i];
    int ruleIndex = static_cast<int>(i);

    int matchedSpecificity = -1;
    for (const Selector &selector : rule.selectors) {
      // A selector's pseudo-element (if any) only ever lives on its last
      // compound (see PseudoElementKind, css.h) - a selector targeting
      // something other than what this call is resolving for doesn't
      // compete at all, the same way a selector for a completely
      // different tag/class wouldn't.
      PseudoElementKind selectorTarget = selector.compounds.empty()
                                              ? PseudoElementKind::kNone
                                              : selector.compounds.back().pseudoElement;
      if (selectorTarget != target) {
        continue;
      }
      if (MatchesChain(selector, node, pseudoState)) {
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
    if (rule.declarations.hasJustifyItems &&
        justifyItemsWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasJustifyItems = true;
      own.justifyItems = rule.declarations.justifyItems;
    }
    if (rule.declarations.hasAlignContent &&
        alignContentWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasAlignContent = true;
      own.alignContent = rule.declarations.alignContent;
    }
    if (rule.declarations.hasGap && gapWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasGap = true;
      own.gap = rule.declarations.gap;
    }
    if (rule.declarations.hasColumnGap &&
        columnGapWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasColumnGap = true;
      own.columnGap = rule.declarations.columnGap;
    }
    if (rule.declarations.hasRowGap &&
        rowGapWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasRowGap = true;
      own.rowGap = rule.declarations.rowGap;
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
    if (rule.declarations.hasGridTemplateColumns &&
        gridTemplateColumnsWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasGridTemplateColumns = true;
      own.gridTemplateColumns = rule.declarations.gridTemplateColumns;
      own.gridTemplateColumnsSubgrid = rule.declarations.gridTemplateColumnsSubgrid;
    }
    if (rule.declarations.hasGridTemplateRows &&
        gridTemplateRowsWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasGridTemplateRows = true;
      own.gridTemplateRows = rule.declarations.gridTemplateRows;
    }
    if (rule.declarations.hasGridTemplateAreas &&
        gridTemplateAreasWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasGridTemplateAreas = true;
      own.gridTemplateAreas = rule.declarations.gridTemplateAreas;
    }
    if (!rule.declarations.gridArea.empty() &&
        gridAreaWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.gridArea = rule.declarations.gridArea;
    }
    if (rule.declarations.hasGridColumn &&
        gridColumnWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasGridColumn = true;
      own.gridColumn = rule.declarations.gridColumn;
    }
    if (rule.declarations.hasGridRow &&
        gridRowWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasGridRow = true;
      own.gridRow = rule.declarations.gridRow;
    }
    if (rule.declarations.hasJustifySelf &&
        justifySelfWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasJustifySelf = true;
      own.justifySelf = rule.declarations.justifySelf;
    }
    if (rule.declarations.hasAlignSelf &&
        alignSelfWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasAlignSelf = true;
      own.alignSelf = rule.declarations.alignSelf;
    }
    if (rule.declarations.hasGridAutoFlow &&
        gridAutoFlowWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasGridAutoFlow = true;
      own.gridAutoFlow = rule.declarations.gridAutoFlow;
    }
    if (rule.declarations.hasContent &&
        contentWin.ShouldTake(matchedSpecificity, ruleIndex)) {
      own.hasContent = true;
      own.content = rule.declarations.content;
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
