#pragma once

#include <string>

#include "widget.h"

namespace artisan {

constexpr Color kDefaultTextColor{0, 0, 0};     // Black.
constexpr Color kDefaultBorderColor{0, 0, 0};   // Black.
constexpr Color kSelectionColor{0xB4, 0xD7, 0xFF}; // Standard-ish selection blue.

// Backend-agnostic drawing primitives needed to paint a laid-out document.
// Implementations own the actual drawing surface (a Skia canvas, etc.) and
// know nothing about HTML or the DOM - swap implementations to change how
// artisan renders without touching layout code.
class IRenderer {
public:
  virtual ~IRenderer() = default;

  virtual void DrawText(const std::string &text, float x, float y,
                         float fontSize, bool bold, const Color &color) = 0;

  // bold affects real font metrics (a bold face isn't just a thicker
  // paint), so measurement needs it too, not just drawing.
  virtual float MeasureText(const std::string &text, float fontSize,
                             bool bold) const = 0;

  virtual float LineHeight(float fontSize, bool bold) const = 0;

  // Strokes an unfilled rectangle outline - the border for boxy controls
  // (input, button), table cells, and the bar used for a horizontal rule.
  virtual void DrawRect(float x, float y, float width, float height,
                         float strokeWidth, const Color &color) = 0;

  // Paints a solid filled rectangle - text selection highlight, the
  // text-entry caret, and a CSS background-color.
  virtual void DrawFilledRect(float x, float y, float width, float height,
                               const Color &color) = 0;

  // Decodes `data` (raw encoded image bytes) and paints it at (x, y).
  // `explicitWidth`/`explicitHeight` (0 = unspecified) come from the
  // markup's width/height attributes: with both set, the image is painted
  // at exactly that size; with one set, the other is derived from the
  // decoded image's aspect ratio; with neither set, the image is scaled
  // down (preserving aspect ratio, never scaled up) to fit maxWidth.
  // Returns the height it was painted at, so callers can advance their
  // layout cursor - or 0 if the data couldn't be decoded.
  virtual float DrawImage(const unsigned char *data, int dataSize, float x,
                           float y, float maxWidth, float explicitWidth,
                           float explicitHeight) = 0;
};

} // namespace artisan
