#pragma once

#include <string>

namespace artisan {

// Backend-agnostic drawing primitives needed to paint a laid-out document.
// Implementations own the actual drawing surface (a Skia canvas, etc.) and
// know nothing about HTML or the DOM - swap implementations to change how
// artisan renders without touching layout code.
class IRenderer {
public:
  virtual ~IRenderer() = default;

  virtual void DrawText(const std::string &text, float x, float y,
                         float fontSize) = 0;

  virtual float MeasureText(const std::string &text, float fontSize) const = 0;

  virtual float LineHeight(float fontSize) const = 0;

  // Strokes an unfilled rectangle outline - the border for boxy controls
  // (input, button) and the bar used for a horizontal rule.
  virtual void DrawRect(float x, float y, float width, float height) = 0;

  // Paints a solid filled rectangle - text selection highlight and the
  // text-entry caret. The one place this renderer uses a color besides
  // black/white: a selection you can't see isn't a functional selection.
  virtual void DrawFilledRect(float x, float y, float width,
                               float height) = 0;

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
