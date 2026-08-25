#pragma once

#include "renderer.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkTypeface.h"

namespace artisan {

// IRenderer implementation that paints text onto a Skia canvas.
class SkiaRenderer : public IRenderer {
public:
  // `canvas` is not owned and must outlive this renderer. May be nullptr
  // if this instance will only ever be used for MeasureText/LineHeight
  // (font metrics don't need a canvas) - e.g. hit-testing a click against
  // text between frames, with no canvas to paint into. Calling any Draw*
  // method on such an instance is a bug, not a graceful no-op.
  explicit SkiaRenderer(SkCanvas *canvas);

  void DrawText(const std::string &text, float x, float y, float fontSize,
                bool bold, const Color &color) override;
  float MeasureText(const std::string &text, float fontSize,
                     bool bold) const override;
  float LineHeight(float fontSize, bool bold) const override;
  void DrawRect(float x, float y, float width, float height,
                float strokeWidth, const Color &color) override;
  void DrawFilledRect(float x, float y, float width, float height,
                       const Color &color) override;
  float DrawImage(const unsigned char *data, int dataSize, float x, float y,
                   float maxWidth, float explicitWidth,
                   float explicitHeight) override;

private:
  const sk_sp<SkTypeface> &TypefaceFor(bool bold) const;

  SkCanvas *canvas_;
  sk_sp<SkTypeface> typeface_;
  sk_sp<SkTypeface> boldTypeface_;
};

} // namespace artisan
