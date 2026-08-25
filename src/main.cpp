#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <SDL2/SDL.h>

#include <iostream>

constexpr int kWidth = 800;
constexpr int kHeight = 600;

int main(int argc, char *argv[]) {
  auto imageInfo = SkImageInfo::Make(
      kWidth, kHeight, kRGBA_8888_SkColorType, kPremul_SkAlphaType);

  auto surface = SkSurfaces::Raster(imageInfo);

  if (!surface) {
    std::cerr << "Failed to create SkSurface\n";
    return 1;
  }

  auto *canvas = surface->getCanvas();

  canvas->clear(SK_ColorWHITE);

  SkPaint paint;
  paint.setColor(SK_ColorRED);

  canvas->drawRect(SkRect::MakeXYWH(100, 100, 300, 200), paint);

  SkPixmap pixmap;
  if (!surface->peekPixels(&pixmap)) {
    std::cerr << "Failed to read back pixels\n";
    return 1;
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
    return 1;
  }

  SDL_Window *window =
      SDL_CreateWindow("artisan", SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED, kWidth, kHeight, 0);

  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  SDL_Texture *texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                         SDL_TEXTUREACCESS_STATIC, kWidth, kHeight);

  SDL_UpdateTexture(texture, nullptr, pixmap.addr(), pixmap.rowBytes());

  bool running = true;
  SDL_Event event;

  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT ||
          (event.type == SDL_KEYDOWN &&
           event.key.keysym.sym == SDLK_ESCAPE)) {
        running = false;
      }
    }

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
