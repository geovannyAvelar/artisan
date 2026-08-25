#include "app.h"
#include "app_script.h"
#include "compiled_document.h"
#include "dom_node.h"
#include "js_engine.h"
#include "skia_renderer.h"
#include "widget.h"
#include "widget_renderer.h"
#include "widget_tree_builder.h"

#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using artisan::BoxRegion;
using artisan::InputFocus;
using artisan::Node;

constexpr int kInitialWidth = 800;
constexpr int kInitialHeight = 600;

std::string GetValue(const Node *node) {
  const std::string *value = node->GetAttribute("value");
  return value != nullptr ? *value : std::string();
}

// The document is compiled from whatever markup bundle the build was
// configured with (ARTISAN_UI_SOURCES / repeatable --html) by artisanc at
// build time (CompiledPages(), in the generated translation unit) - the
// same pipeline that used to produce a separate, inert, immutable Widget
// tree now produces these ordinary mutable Node trees instead, one per
// page. Whichever page is currently live gets its behavior from two
// independent sources that both re-run every time a page loads (first at
// startup, then again on every Navigate()) and both drive that tree
// through the same Node API: SetupApp (app.h), plain compiled C++ from
// whichever sources the build was configured with
// (ARTISAN_APP_CPP_SOURCES / --cpp); and GetAppScript() (below),
// interpreted by JsEngine, from whichever .js file it was configured
// with (ARTISAN_APP_JS_SOURCE / --js, "" if none). Neither knows the
// other exists - proof that "native C++" and "script" are just two ways
// to drive the DOM, not a fork in the architecture. Both are shared
// across every page in the bundle (there's exactly one app.cpp/app.js
// configured, not one per page) - a page missing the ids they look for
// is simply left alone, the same way SetupApp already tolerates that.

// Erases the focus's selected range (if any) from `value` and collapses
// the cursor to where the selection started. Returns whether there was
// anything to erase, so callers can tell "selection deleted" apart from
// "nothing was selected, do the normal single-character thing instead".
bool EraseSelection(InputFocus &focus, std::string &value) {
  int start = std::min(focus.cursorPos, focus.selectionAnchor);
  int end = std::max(focus.cursorPos, focus.selectionAnchor);
  if (start == end) {
    return false;
  }
  value.erase(static_cast<size_t>(start), static_cast<size_t>(end - start));
  focus.cursorPos = start;
  focus.selectionAnchor = start;
  return true;
}

// Materializes `document` (with `focus`'s caret/selection, if any) and
// paints the *whole* thing - not just the viewport - into a new texture
// the caller owns, sized (width, max(height, content height)). Scrolling
// then just picks which vertical window of that texture SDL_RenderCopy
// shows (see the main loop below); nothing here needs to know the scroll
// position, so a wheel/key scroll never has to re-run layout.
//
// Also records where every input/button box landed, in the same
// unscrolled content coordinates - the next click or drag hit-tests
// against these after adding the current scroll offset back in.
SDL_Texture *RenderFrame(SDL_Renderer *renderer,
                          const artisan::WidgetRenderer &measuringWidgetRenderer,
                          const Node &document, const InputFocus &focus,
                          int width, int height,
                          std::vector<BoxRegion> *outBoxRegions,
                          float *outContentHeight) {
  auto widgetTree = artisan::BuildWidgetTree(document, focus);

  float contentHeight =
      measuringWidgetRenderer.MeasureContentHeight(widgetTree->root(), width);
  int surfaceHeight = std::max(height, static_cast<int>(std::ceil(contentHeight)));
  *outContentHeight = contentHeight;

  auto imageInfo = SkImageInfo::Make(width, surfaceHeight,
                                      kRGBA_8888_SkColorType,
                                      kPremul_SkAlphaType);

  auto surface = SkSurfaces::Raster(imageInfo);
  if (!surface) {
    std::cerr << "Failed to create SkSurface\n";
    return nullptr;
  }

  auto *canvas = surface->getCanvas();
  canvas->clear(SK_ColorWHITE);

  artisan::SkiaRenderer skiaRenderer(canvas);
  artisan::WidgetRenderer widgetRenderer(skiaRenderer);
  widgetRenderer.Render(widgetTree->root(), width, outBoxRegions);

  SkPixmap pixmap;
  if (!surface->peekPixels(&pixmap)) {
    std::cerr << "Failed to read back pixels\n";
    return nullptr;
  }

  SDL_Texture *texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                         SDL_TEXTUREACCESS_STATIC, width, surfaceHeight);
  if (texture == nullptr) {
    std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << "\n";
    return nullptr;
  }

  SDL_UpdateTexture(texture, nullptr, pixmap.addr(), pixmap.rowBytes());
  return texture;
}

float MaxScroll(float contentHeight, int viewportHeight) {
  return std::max(0.0f, contentHeight - static_cast<float>(viewportHeight));
}

// A thin native (non-Skia) scrollbar thumb drawn straight onto the SDL
// renderer as UI chrome, on top of the content texture - not part of the
// document, so it doesn't belong in the compiled/rendered tree at all.
void DrawScrollbar(SDL_Renderer *renderer, int viewportWidth,
                    int viewportHeight, float scrollY, float contentHeight) {
  if (contentHeight <= static_cast<float>(viewportHeight)) {
    return;
  }

  constexpr int kBarWidth = 6;
  constexpr int kBarMargin = 2;
  constexpr float kMinThumbHeight = 24.0f;

  float trackHeight = static_cast<float>(viewportHeight);
  float thumbHeight = std::max(
      kMinThumbHeight, trackHeight * (trackHeight / contentHeight));
  float maxScroll = MaxScroll(contentHeight, viewportHeight);
  float thumbY = maxScroll > 0.0f
                     ? (scrollY / maxScroll) * (trackHeight - thumbHeight)
                     : 0.0f;

  SDL_Rect thumbRect{viewportWidth - kBarWidth - kBarMargin,
                      static_cast<int>(thumbY), kBarWidth,
                      static_cast<int>(thumbHeight)};

  SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
  SDL_RenderFillRect(renderer, &thumbRect);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
}

const BoxRegion *FindRegion(const std::vector<BoxRegion> &regions,
                             const Node *node) {
  for (const BoxRegion &region : regions) {
    if (region.userData == node) {
      return &region;
    }
  }
  return nullptr;
}

// The box (if any) at (x, y), using the last render's positions.
const BoxRegion *HitTestRegion(const std::vector<BoxRegion> &regions,
                                float x, float y) {
  for (const BoxRegion &region : regions) {
    if (x >= region.x && x <= region.x + region.width && y >= region.y &&
        y <= region.y + region.height) {
      return &region;
    }
  }
  return nullptr;
}

// Turns a screen point into a character offset within `node`'s value,
// using the same font metrics BuildWidgetTree renders it with -
// `renderer` must be a measurement-only IRenderer (see SkiaRenderer's
// nullptr-canvas constructor note); this never paints with it.
int CharIndexAt(const artisan::IRenderer &renderer, const BoxRegion &region,
                 const Node *node, float x) {
  float relativeX = x - (region.x + artisan::kBoxPadding);
  return artisan::CharIndexAtX(renderer, GetValue(node),
                                artisan::kDefaultFontSize, relativeX);
}

int main(int argc, char *argv[]) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow(
      "artisan", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      kInitialWidth, kInitialHeight, SDL_WINDOW_RESIZABLE);

  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  SDL_StartTextInput();

  // Never used to paint - only its MeasureText (font metrics don't need a
  // canvas) backs CharIndexAt for hit-testing clicks/drags between frames,
  // and (via WidgetRenderer::MeasureContentHeight) sizing the content
  // surface before painting it.
  artisan::SkiaRenderer measuringRenderer(nullptr);
  artisan::WidgetRenderer measuringWidgetRenderer(measuringRenderer);

  const std::vector<artisan::PageDescriptor> &pages = artisan::CompiledPages();
  if (pages.empty()) {
    std::cerr << "main: the compiled bundle has no pages\n";
    return 1;
  }

  std::unique_ptr<Node> document;

  // jsEngine must outlive `document`: click handlers registered below
  // hold QuickJS function references (JsCallback) that get released when
  // the Node owning them is destroyed, and that release needs a live
  // JSContext. A unique_ptr (rather than a stack object) lets Navigate()
  // below tear down and rebuild both per page-visit, in the safe order:
  // release the old document (and every JsCallback it holds) before
  // releasing the old jsEngine, then build the new page and construct a
  // fresh jsEngine bound to it. The same ordering happens once more, via
  // the explicit document.reset() right before main returns: it destroys
  // the final Node tree while jsEngine/its JSContext are still alive,
  // before jsEngine's own (now safe) destructor runs implicitly after.
  std::unique_ptr<artisan::JsEngine> jsEngine;

  InputFocus focus;
  bool isSelecting = false;

  float scrollY = 0.0f;
  float contentHeight = static_cast<float>(kInitialHeight);
  constexpr float kWheelStep = 40.0f;
  constexpr float kArrowStep = 40.0f;

  int width = kInitialWidth;
  int height = kInitialHeight;
  std::vector<BoxRegion> boxRegions;

  bool running = true;
  bool needsRedraw = false;

  // Tears down whichever page is currently live (if any) and builds
  // `pageName` fresh in its place: a new Node tree, SetupApp rewired
  // against it, and the embedded script re-run against it - the same
  // startup sequence every page goes through, including the first one
  // (called once below, outside the event loop). Does nothing but log if
  // `pageName` isn't in the bundle, same as SetupApp's own missing-id
  // tolerance.
  auto navigate = [&](std::string pageName) {
    const artisan::PageDescriptor *page = nullptr;
    for (const artisan::PageDescriptor &candidate : pages) {
      if (candidate.name == pageName) {
        page = &candidate;
        break;
      }
    }
    if (page == nullptr) {
      std::cerr << "main: no such page \"" << pageName << "\"\n";
      return;
    }

    document.reset();
    jsEngine.reset();

    document = page->build();
    artisan::SetupApp(*document);
    jsEngine = std::make_unique<artisan::JsEngine>(*document);
    std::string appScript = artisan::GetAppScript();
    if (!appScript.empty() && !jsEngine->RunScript(appScript, "app.js")) {
      std::cerr << "main: the app script failed to run\n";
    }

    focus = InputFocus{};
    isSelecting = false;
    scrollY = 0.0f;
    needsRedraw = true;
  };

  navigate(pages.front().name);

  SDL_Texture *texture =
      RenderFrame(renderer, measuringWidgetRenderer, *document, focus, width,
                  height, &boxRegions, &contentHeight);
  if (texture == nullptr) {
    return 1;
  }
  needsRedraw = false;

  SDL_Event event;

  while (running) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
        running = false;
        break;

      case SDL_KEYDOWN: {
        const bool shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;
        const bool ctrl = (event.key.keysym.mod & KMOD_CTRL) != 0;

        if (event.key.keysym.sym == SDLK_ESCAPE) {
          running = false;
          break;
        }

        if (focus.node == nullptr) {
          // Nothing focused to type into - arrow/page/home/end keys
          // scroll the page instead, same as a browser with no field
          // active.
          float maxScroll = MaxScroll(contentHeight, height);
          switch (event.key.keysym.sym) {
          case SDLK_UP:
            scrollY -= kArrowStep;
            break;
          case SDLK_DOWN:
            scrollY += kArrowStep;
            break;
          case SDLK_PAGEUP:
            scrollY -= static_cast<float>(height);
            break;
          case SDLK_PAGEDOWN:
            scrollY += static_cast<float>(height);
            break;
          case SDLK_HOME:
            scrollY = 0.0f;
            break;
          case SDLK_END:
            scrollY = maxScroll;
            break;
          default:
            break;
          }
          scrollY = std::clamp(scrollY, 0.0f, maxScroll);
          break;
        }

        std::string value = GetValue(focus.node);
        bool valueChanged = false;

        switch (event.key.keysym.sym) {
        case SDLK_BACKSPACE:
          if (!EraseSelection(focus, value) && focus.cursorPos > 0) {
            value.erase(static_cast<size_t>(focus.cursorPos - 1), 1);
            focus.cursorPos -= 1;
            focus.selectionAnchor = focus.cursorPos;
          }
          valueChanged = true;
          break;

        case SDLK_DELETE:
          if (!EraseSelection(focus, value) &&
              focus.cursorPos < static_cast<int>(value.size())) {
            value.erase(static_cast<size_t>(focus.cursorPos), 1);
          }
          valueChanged = true;
          break;

        case SDLK_LEFT:
          if (!shift && focus.cursorPos != focus.selectionAnchor) {
            focus.cursorPos = std::min(focus.cursorPos, focus.selectionAnchor);
          } else if (focus.cursorPos > 0) {
            focus.cursorPos -= 1;
          }
          if (!shift) {
            focus.selectionAnchor = focus.cursorPos;
          }
          needsRedraw = true;
          break;

        case SDLK_RIGHT:
          if (!shift && focus.cursorPos != focus.selectionAnchor) {
            focus.cursorPos = std::max(focus.cursorPos, focus.selectionAnchor);
          } else if (focus.cursorPos < static_cast<int>(value.size())) {
            focus.cursorPos += 1;
          }
          if (!shift) {
            focus.selectionAnchor = focus.cursorPos;
          }
          needsRedraw = true;
          break;

        case SDLK_HOME:
          focus.cursorPos = 0;
          if (!shift) {
            focus.selectionAnchor = focus.cursorPos;
          }
          needsRedraw = true;
          break;

        case SDLK_END:
          focus.cursorPos = static_cast<int>(value.size());
          if (!shift) {
            focus.selectionAnchor = focus.cursorPos;
          }
          needsRedraw = true;
          break;

        case SDLK_a:
          if (ctrl) {
            focus.selectionAnchor = 0;
            focus.cursorPos = static_cast<int>(value.size());
            needsRedraw = true;
          }
          break;

        case SDLK_c:
        case SDLK_x: {
          if (!ctrl) {
            break;
          }
          int start = std::min(focus.cursorPos, focus.selectionAnchor);
          int end = std::max(focus.cursorPos, focus.selectionAnchor);
          if (end <= start) {
            break;
          }
          SDL_SetClipboardText(
              value.substr(static_cast<size_t>(start),
                            static_cast<size_t>(end - start))
                  .c_str());
          if (event.key.keysym.sym == SDLK_x) {
            EraseSelection(focus, value);
            valueChanged = true;
          }
          break;
        }

        case SDLK_v:
          if (ctrl && SDL_HasClipboardText()) {
            char *clipboard = SDL_GetClipboardText();
            EraseSelection(focus, value);
            std::string pasted(clipboard);
            value.insert(static_cast<size_t>(focus.cursorPos), pasted);
            focus.cursorPos += static_cast<int>(pasted.size());
            focus.selectionAnchor = focus.cursorPos;
            SDL_free(clipboard);
            valueChanged = true;
          }
          break;

        default:
          break;
        }

        if (valueChanged) {
          focus.node->SetAttribute("value", value);
          needsRedraw = true;
        }
        break;
      }

      case SDL_TEXTINPUT:
        if (focus.node != nullptr) {
          std::string value = GetValue(focus.node);
          EraseSelection(focus, value);
          value.insert(static_cast<size_t>(focus.cursorPos), event.text.text);
          focus.cursorPos += static_cast<int>(std::string(event.text.text).size());
          focus.selectionAnchor = focus.cursorPos;
          focus.node->SetAttribute("value", value);
          needsRedraw = true;
        }
        break;

      case SDL_MOUSEBUTTONDOWN:
        if (event.button.button == SDL_BUTTON_LEFT) {
          // boxRegions are in unscrolled content coordinates; the click
          // is in screen coordinates - add the scroll offset back in to
          // compare like with like.
          const BoxRegion *region = HitTestRegion(
              boxRegions, static_cast<float>(event.button.x),
              static_cast<float>(event.button.y) + scrollY);
          auto *node = region != nullptr
                           ? static_cast<Node *>(
                                 const_cast<void *>(region->userData))
                           : nullptr;

          if (node != nullptr && node->tagName() == "input") {
            int index = CharIndexAt(measuringRenderer, *region, node,
                                     static_cast<float>(event.button.x));
            focus.node = node;
            focus.cursorPos = index;
            focus.selectionAnchor = index;
            isSelecting = true;
          } else {
            // Clicking a button/link (or anything else) blurs whatever
            // input was focused, same as a real page - do that before
            // Click()/navigate() runs, so a handler that edits the
            // just-blurred input's value isn't fighting a stale
            // focus/cursor still pointing at it.
            focus = InputFocus{};
            isSelecting = false;
            if (node != nullptr && node->tagName() == "button") {
              node->Click();
            } else if (node != nullptr && node->tagName() == "a") {
              const std::string *href = node->GetAttribute("href");
              if (href != nullptr) {
                navigate(*href);
              }
            }
          }
          needsRedraw = true;
        }
        break;

      case SDL_MOUSEMOTION:
        if (isSelecting && focus.node != nullptr) {
          const BoxRegion *region = FindRegion(boxRegions, focus.node);
          if (region != nullptr) {
            int index = CharIndexAt(measuringRenderer, *region, focus.node,
                                     static_cast<float>(event.motion.x));
            if (index != focus.cursorPos) {
              focus.cursorPos = index;
              needsRedraw = true;
            }
          }
        }
        break;

      case SDL_MOUSEBUTTONUP:
        if (event.button.button == SDL_BUTTON_LEFT) {
          isSelecting = false;
        }
        break;

      case SDL_MOUSEWHEEL: {
        // SDL_MOUSEWHEEL_FLIPPED reverses the sign of y (some
        // touchpads/platforms report "natural" scrolling that way) - undo
        // that so kWheelStep's sign convention stays consistent either
        // way: wheel.y > 0 (scrolled away from the user) moves the
        // viewport down through the content.
        float direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                               ? -1.0f
                               : 1.0f;
        scrollY += direction * static_cast<float>(event.wheel.y) *
                   -kWheelStep;
        scrollY = std::clamp(scrollY, 0.0f, MaxScroll(contentHeight, height));
        break;
      }

      case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          width = event.window.data1;
          height = event.window.data2;
          scrollY = std::clamp(scrollY, 0.0f, MaxScroll(contentHeight, height));
          needsRedraw = true;
        }
        break;

      default:
        break;
      }
    }

    if (needsRedraw) {
      SDL_Texture *newTexture =
          RenderFrame(renderer, measuringWidgetRenderer, *document, focus,
                      width, height, &boxRegions, &contentHeight);
      if (newTexture != nullptr) {
        SDL_DestroyTexture(texture);
        texture = newTexture;
      }
      needsRedraw = false;
      // The document may have grown/shrunk (a handler changed content, or
      // the window was resized) - keep the scroll position in range.
      scrollY = std::clamp(scrollY, 0.0f, MaxScroll(contentHeight, height));
    }

    SDL_RenderClear(renderer);
    SDL_Rect srcRect{0, static_cast<int>(scrollY), width, height};
    SDL_RenderCopy(renderer, texture, &srcRect, nullptr);
    DrawScrollbar(renderer, width, height, scrollY, contentHeight);
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  // Destroy the Node tree (and every JsCallback its click handlers hold)
  // now, while jsEngine is still alive - see the comment where jsEngine
  // is constructed above.
  document.reset();

  return 0;
}
