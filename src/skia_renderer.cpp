#include "skia_renderer.h"

#include "include/codec/SkGifDecoder.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/codec/SkPngDecoder.h"
#include "include/codec/SkWebpDecoder.h"
#include "include/core/SkData.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/ports/SkFontMgr_fontconfig.h"
#include "include/ports/SkFontScanner_FreeType.h"

#include <algorithm>

namespace artisan {

namespace {

SkFont MakeFont(const sk_sp<SkTypeface> &typeface, float fontSize) {
  return SkFont(typeface, fontSize);
}

// SkCodecs::Register is not thread-safe and must happen before the first
// decode; SkiaRenderer is constructed once on the main thread before any
// rendering happens, so doing it there is enough.
void RegisterImageCodecsOnce() {
  static bool registered = [] {
    SkCodecs::Register(SkPngDecoder::Decoder());
    SkCodecs::Register(SkJpegDecoder::Decoder());
    SkCodecs::Register(SkWebpDecoder::Decoder());
    SkCodecs::Register(SkGifDecoder::Decoder());
    return true;
  }();
  (void)registered;
}

} // namespace

SkiaRenderer::SkiaRenderer(SkCanvas *canvas) : canvas_(canvas) {
  sk_sp<SkFontMgr> fontMgr =
      SkFontMgr_New_FontConfig(nullptr, SkFontScanner_Make_FreeType());
  typeface_ = fontMgr->legacyMakeTypeface(nullptr, SkFontStyle());

  RegisterImageCodecsOnce();
}

void SkiaRenderer::DrawText(const std::string &text, float x, float y,
                             float fontSize) {
  SkPaint paint;
  paint.setColor(SK_ColorBLACK);
  paint.setAntiAlias(true);

  canvas_->drawString(text.c_str(), x, y, MakeFont(typeface_, fontSize),
                       paint);
}

float SkiaRenderer::MeasureText(const std::string &text,
                                 float fontSize) const {
  return MakeFont(typeface_, fontSize)
      .measureText(text.c_str(), text.size(), SkTextEncoding::kUTF8);
}

float SkiaRenderer::LineHeight(float fontSize) const {
  return MakeFont(typeface_, fontSize).getSpacing();
}

void SkiaRenderer::DrawRect(float x, float y, float width, float height) {
  SkPaint paint;
  paint.setColor(SK_ColorBLACK);
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(1.0f);

  canvas_->drawRect(SkRect::MakeXYWH(x, y, width, height), paint);
}

void SkiaRenderer::DrawFilledRect(float x, float y, float width,
                                   float height) {
  SkPaint paint;
  paint.setColor(SkColorSetRGB(0xB4, 0xD7, 0xFF)); // Standard-ish selection blue.
  paint.setStyle(SkPaint::kFill_Style);

  canvas_->drawRect(SkRect::MakeXYWH(x, y, width, height), paint);
}

float SkiaRenderer::DrawImage(const unsigned char *data, int dataSize,
                               float x, float y, float maxWidth,
                               float explicitWidth, float explicitHeight) {
  if (data == nullptr || dataSize <= 0) {
    return 0.0f;
  }

  sk_sp<SkData> encoded =
      SkData::MakeWithoutCopy(data, static_cast<size_t>(dataSize));
  sk_sp<SkImage> image = SkImages::DeferredFromEncodedData(encoded);
  if (!image) {
    return 0.0f;
  }

  const float intrinsicWidth = static_cast<float>(image->width());
  const float intrinsicHeight = static_cast<float>(image->height());

  float width;
  float height;

  if (explicitWidth > 0.0f && explicitHeight > 0.0f) {
    width = explicitWidth;
    height = explicitHeight;
  } else if (explicitWidth > 0.0f) {
    width = explicitWidth;
    height = explicitWidth * intrinsicHeight / intrinsicWidth;
  } else if (explicitHeight > 0.0f) {
    height = explicitHeight;
    width = explicitHeight * intrinsicWidth / intrinsicHeight;
  } else {
    float scale = std::min(1.0f, maxWidth / intrinsicWidth);
    width = intrinsicWidth * scale;
    height = intrinsicHeight * scale;
  }

  canvas_->drawImageRect(image.get(), SkRect::MakeXYWH(x, y, width, height),
                          SkSamplingOptions(), nullptr);

  return height;
}

} // namespace artisan
