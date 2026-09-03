#include "app.h"
#include "app_script.h"
#include "art_bridge_context.h"
#include "compiled_document.h"
#include "css.h"
#include "dom_node.h"
#include "js_engine.h"
#include "node_c_api.h"
#include "node_c_api_bridge.h"
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
#include <cctype>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using artisan::AnimationFrameQueue;
using artisan::BoxRegion;
using artisan::InputFocus;
using artisan::KeyEventInit;
using artisan::MouseEventInit;
using artisan::Node;
using artisan::TimerQueue;

constexpr int kInitialWidth = 800;
constexpr int kInitialHeight = 600;

std::string GetValue(const Node *node) {
  const std::string *value = node->GetAttribute("value");
  return value != nullptr ? *value : std::string();
}

// Unchecks every other <input type="radio"> under `subtree` that shares
// `justChecked`'s `name` attribute - real radio-group semantics (only one
// checked per group), enforced here at click time rather than via any
// generic "query all matching" API, since nothing else in this codebase
// needs one yet (Node::FindById is the only lookup primitive - see
// dom_node.h).
void UncheckOtherRadiosInGroup(Node &subtree, Node *justChecked) {
  const std::string *groupName = justChecked->GetAttribute("name");
  if (groupName == nullptr) {
    return;
  }

  for (const auto &childPtr : subtree.children()) {
    Node *child = childPtr.get();
    if (child != justChecked && child->tagName() == "input") {
      const std::string *type = child->GetAttribute("type");
      const std::string *name = child->GetAttribute("name");
      if (type != nullptr && *type == "radio" && name != nullptr &&
          *name == *groupName) {
        child->RemoveAttribute("checked");
      }
    }
    UncheckOtherRadiosInGroup(*child, justChecked);
  }
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
// (ARTISAN_APP_CPP_SOURCES / --cpp); GetAppScript() (below), interpreted
// by JsEngine, from whichever .js file it was configured with
// (ARTISAN_APP_JS_SOURCE / --js, "" if none); and ArtisanSetupApp
// (node_c_api.h), a Go app compiled ahead of time from whichever
// directory it was configured with (ARTISAN_APP_GO_SOURCE / --go, a
// no-op stub if none). None of the three knows the others exist - proof
// that "native C++", "script", and "native Go" are just three ways to
// drive the DOM, not a fork in the architecture. All three are shared
// across every page in the bundle (there's exactly one app.cpp/app.js/Go
// app configured, not one per page) - a page missing the ids they look
// for is simply left alone, the same way SetupApp already tolerates that.

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
// Also records where every input/button/hoverable box landed, in the
// same unscrolled content coordinates - the next click or hover
// hit-tests against these after adding the current scroll offset back
// in. `pseudoState` is what `:hover`/`:focus` in a `<style>` block match
// against for this render - the caller (the main loop below) is the one
// that actually knows which node is hovered/focused.
SDL_Texture *RenderFrame(SDL_Renderer *renderer,
                          const artisan::WidgetRenderer &measuringWidgetRenderer,
                          const Node &document, const InputFocus &focus,
                          const artisan::PseudoClassState &pseudoState,
                          int width, int height,
                          std::vector<BoxRegion> *outBoxRegions,
                          float *outContentHeight) {
  auto widgetTree = artisan::BuildWidgetTree(document, focus, pseudoState);

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

// A MouseEventInit for a click at `x, y` (screen coordinates, matching
// SDL's own mouse-event coordinates), with the current modifier keys
// read from SDL's global keyboard state - shared by every mouse-driven
// "click" dispatch below (a direct click, a button/checkbox/radio
// activation, or one forwarded through a <label for="...">).
MouseEventInit MakeMouseEventInit(float x, float y) {
  MouseEventInit init;
  init.clientX = x;
  init.clientY = y;
  SDL_Keymod mod = SDL_GetModState();
  init.ctrlKey = (mod & KMOD_CTRL) != 0;
  init.shiftKey = (mod & KMOD_SHIFT) != 0;
  init.altKey = (mod & KMOD_ALT) != 0;
  init.metaKey = (mod & KMOD_GUI) != 0;
  return init;
}

// DOM-style key/code names for the keys SDL_KEYDOWN's own switch below
// already recognizes - "key" is the logical key (what real DOM's
// KeyboardEvent.key would report for the *unshifted* case; this engine
// doesn't track shift-remapped punctuation like "1" -> "!", a known,
// accepted imprecision), "code" is the physical key (identical to `key`
// for every non-printable name here, matching real DOM). A printable
// ASCII key (letters, digits, common punctuation) falls back to its own
// character as `key`, and a matching "KeyX"/"DigitX" as `code` - close
// enough to real DOM's actual scancode-based naming without needing a
// full SDL_Scancode table.
KeyEventInit MakeKeyEventInit(const SDL_Keysym &keysym) {
  KeyEventInit init;
  SDL_Keycode sym = keysym.sym;
  switch (sym) {
  case SDLK_BACKSPACE:
    init.key = init.code = "Backspace";
    break;
  case SDLK_DELETE:
    init.key = init.code = "Delete";
    break;
  case SDLK_LEFT:
    init.key = init.code = "ArrowLeft";
    break;
  case SDLK_RIGHT:
    init.key = init.code = "ArrowRight";
    break;
  case SDLK_UP:
    init.key = init.code = "ArrowUp";
    break;
  case SDLK_DOWN:
    init.key = init.code = "ArrowDown";
    break;
  case SDLK_HOME:
    init.key = init.code = "Home";
    break;
  case SDLK_END:
    init.key = init.code = "End";
    break;
  case SDLK_PAGEUP:
    init.key = init.code = "PageUp";
    break;
  case SDLK_PAGEDOWN:
    init.key = init.code = "PageDown";
    break;
  case SDLK_TAB:
    init.key = init.code = "Tab";
    break;
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    init.key = init.code = "Enter";
    break;
  case SDLK_ESCAPE:
    init.key = init.code = "Escape";
    break;
  case SDLK_SPACE:
    init.key = " ";
    init.code = "Space";
    break;
  default:
    if (sym >= 'a' && sym <= 'z') {
      init.key = std::string(1, static_cast<char>(sym));
      init.code = "Key" + std::string(1, static_cast<char>(std::toupper(sym)));
    } else if (sym >= '0' && sym <= '9') {
      init.key = std::string(1, static_cast<char>(sym));
      init.code = "Digit" + init.key;
    } else if (sym >= 32 && sym < 127) {
      init.key = init.code = std::string(1, static_cast<char>(sym));
    }
    break;
  }
  SDL_Keymod mod = SDL_GetModState();
  init.ctrlKey = (mod & KMOD_CTRL) != 0;
  init.shiftKey = (mod & KMOD_SHIFT) != 0;
  init.altKey = (mod & KMOD_ALT) != 0;
  init.metaKey = (mod & KMOD_GUI) != 0;
  return init;
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
  float relativeX = x - (region.x + region.textInsetLeft);
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

  // jsEngine must outlive `document`: event listeners registered below
  // (addEventListener, and SetOnClick/AddEventListener called from a
  // script through it) hold QuickJS function references (JsCallback) that
  // get released when the Node owning them is destroyed, and that release
  // needs a live JSContext. Declared *before* `document` so that's true
  // even on an early-return path that skips navigate()'s/main()'s own
  // explicit teardown sequencing below and just falls through to
  // ordinary scope-exit destruction, which runs in reverse declaration
  // order - document's Nodes (and their JsCallbacks) destroyed first,
  // this (now safe to go) destroyed after. A unique_ptr (rather than a
  // stack object) additionally lets navigate() below tear down and
  // rebuild both per page-visit, in that same safe order: release the
  // old document (and every JsCallback it holds) before releasing the
  // old jsEngine, then build the new page and construct a fresh jsEngine
  // bound to it.
  std::unique_ptr<artisan::JsEngine> jsEngine;

  std::unique_ptr<Node> document;

  // setTimeout/setInterval's backing store - a runtime-loop concern (see
  // timer_queue.h), so it lives here alongside focus/scrollY/etc. rather
  // than inside JsEngine, which only ever schedules/cancels into it.
  // Reset on every navigate() (below), same as focus/scrollY - a timer
  // from a previous page firing into a torn-down Node tree would be a
  // use-after-free.
  TimerQueue timerQueue;

  // requestAnimationFrame's backing store - same reasoning/lifetime as
  // timerQueue above, just a different firing rule (see
  // animation_frame_queue.h).
  AnimationFrameQueue animationFrameQueue;

  InputFocus focus;
  bool isSelecting = false;

  // Set for one event by SDL_KEYDOWN below when a script's "keydown"
  // listener called preventDefault() - real DOM's `keydown`
  // preventDefault() suppresses the browser's own default handling of
  // that key, and for a text field that includes the character SDL is
  // about to report separately via its own SDL_TEXTINPUT event (SDL
  // splits "a key went down" and "this produced text" into two events;
  // real DOM doesn't). Consumed (and cleared) by the very next
  // SDL_TEXTINPUT - see the two cases below.
  bool suppressNextTextInput = false;

  // The node the pointer is currently positioned over, if any - what
  // `:hover` (css.h's PseudoClassState) matches against. Updated by
  // SDL_MOUSEMOTION below via the same boxRegions hit-testing a click
  // already uses; a change here means the *rendered* styling might need
  // to change too (a `:hover` rule taking or losing effect), so it's the
  // one piece of state in this loop that sets needsRedraw on its own
  // rather than only in response to a DOM mutation.
  Node *hoveredNode = nullptr;

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
    artisan::SetArtDocumentContext(nullptr);
    jsEngine.reset();

    document = page->build();
    artisan::SetArtDocumentContext(document.get());
    timerQueue = TimerQueue{};
    animationFrameQueue = AnimationFrameQueue{};
    artisan::SetGoTimerContext(timerQueue, animationFrameQueue);
    artisan::SetArtTimerContext(timerQueue, animationFrameQueue);
    artisan::SetupApp(*document);
    ArtisanSetupApp(reinterpret_cast<ArtisanNode *>(document.get()));
    jsEngine = std::make_unique<artisan::JsEngine>(*document, timerQueue,
                                                     animationFrameQueue);
    std::string jsPreludeScript = artisan::GetJsPreludeScript();
    if (!jsPreludeScript.empty() &&
        !jsEngine->RunScript(jsPreludeScript, "js-prelude.js")) {
      std::cerr << "main: the JS prelude script failed to run\n";
    }
    std::string appScript = artisan::GetAppScript();
    if (!appScript.empty() && !jsEngine->RunScript(appScript, "app.js")) {
      std::cerr << "main: the app script failed to run\n";
    }

    focus = InputFocus{};
    isSelecting = false;
    suppressNextTextInput = false;
    hoveredNode = nullptr;
    scrollY = 0.0f;
    needsRedraw = true;
  };

  // What clicking `target` actually does, dispatched by its tag/type -
  // shared between a direct click on a box/button and a <label for="...">
  // click forwarded to whatever it points at (see the "label" branch in
  // SDL_MOUSEBUTTONDOWN below). No-op if `target` is nullptr (an unresolved
  // `for`) or isn't one of these element/type combinations. `mouseInit`
  // carries the real click's position/modifier keys through to the
  // dispatched "click" (DispatchMouseEvent, not Click() - reaches the
  // exact same listeners either way, Click() being a thin wrapper over
  // plain DispatchEvent("click"), just without anywhere to put this
  // data).
  auto activateControl = [&](Node *target, const MouseEventInit &mouseInit) {
    if (target == nullptr) {
      return;
    }

    if (target->tagName() == "input" && artisan::IsCheckableInputType(*target)) {
      const std::string *type = target->GetAttribute("type");
      bool alreadyChecked = target->GetAttribute("checked") != nullptr;
      bool stateChanged = true;
      if (*type == "radio") {
        // Matches real <input type=radio> behavior: clicking the already-
        // checked radio in a group is a no-op (no state change, no
        // "change" event), only clicking a *different* one does anything.
        stateChanged = !alreadyChecked;
        if (stateChanged) {
          target->SetAttribute("checked", "checked");
          UncheckOtherRadiosInGroup(*document, target);
        }
      } else if (alreadyChecked) {
        target->RemoveAttribute("checked");
      } else {
        target->SetAttribute("checked", "checked");
      }
      // Real checkbox/radio fire both - "change" first (state already
      // updated above), then "click".
      if (stateChanged) {
        target->DispatchEvent("change");
      }
      target->DispatchMouseEvent("click", mouseInit);
    } else if (target->tagName() == "input") {
      // A plain text <input> - the path a <label for="..."> pointing at
      // one takes, same as clicking it directly would (focus, caret at
      // the end of its current value).
      std::string value = GetValue(target);
      focus.node = target;
      focus.cursorPos = static_cast<int>(value.size());
      focus.selectionAnchor = focus.cursorPos;
      isSelecting = false;
    } else if (target->tagName() == "button") {
      target->DispatchMouseEvent("click", mouseInit);
    }
  };

  navigate(pages.front().name);

  SDL_Texture *texture = RenderFrame(
      renderer, measuringWidgetRenderer, *document, focus,
      artisan::PseudoClassState{hoveredNode, focus.node}, width, height,
      &boxRegions, &contentHeight);
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

        // A listener calling preventDefault() here suppresses every bit
        // of this handler's own built-in editing below for this key -
        // backspace/delete, arrow/home/end cursor movement, and the
        // ctrl+a/c/x/v shortcuts alike - the same as a real browser
        // suppressing its default keydown handling on an <input>.
        // suppressNextTextInput carries that same decision one step
        // further, to the character insertion SDL reports via a
        // separate SDL_TEXTINPUT event (see its declaration above).
        bool keyDefaultPrevented = focus.node->DispatchKeyEvent(
            "keydown", MakeKeyEventInit(event.key.keysym));
        suppressNextTextInput = keyDefaultPrevented;
        if (keyDefaultPrevented) {
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
          focus.node->DispatchEvent("input");
          needsRedraw = true;
        }
        break;
      }

      case SDL_TEXTINPUT:
        // Consume, not just check: this suppression is one-shot, for the
        // single character the keydown that set it was about to produce
        // - an unrelated later SDL_TEXTINPUT (from some subsequent key
        // press) should insert normally.
        if (suppressNextTextInput) {
          suppressNextTextInput = false;
          break;
        }
        if (focus.node != nullptr) {
          std::string value = GetValue(focus.node);
          EraseSelection(focus, value);
          value.insert(static_cast<size_t>(focus.cursorPos), event.text.text);
          focus.cursorPos += static_cast<int>(std::string(event.text.text).size());
          focus.selectionAnchor = focus.cursorPos;
          focus.node->SetAttribute("value", value);
          focus.node->DispatchEvent("input");
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
          // clientX/clientY are viewport-relative, matching real DOM -
          // the raw window coordinates, not scrolled-content-adjusted
          // (this engine has no separate pageX/pageY for that).
          MouseEventInit mouseInit = MakeMouseEventInit(
              static_cast<float>(event.button.x), static_cast<float>(event.button.y));

          if (node != nullptr && node->tagName() == "input" &&
              !artisan::IsCheckableInputType(*node)) {
            int index = CharIndexAt(measuringRenderer, *region, node,
                                     static_cast<float>(event.button.x));
            focus.node = node;
            focus.cursorPos = index;
            focus.selectionAnchor = index;
            isSelecting = true;
          } else {
            // Clicking a button/checkbox/radio/label/link (or anything
            // else) blurs whatever input was focused, same as a real page
            // - do that before Click()/navigate() runs, so a handler that
            // edits the just-blurred input's value isn't fighting a stale
            // focus/cursor still pointing at it.
            focus = InputFocus{};
            isSelecting = false;
            if (node != nullptr &&
                (node->tagName() == "button" ||
                 (node->tagName() == "input" &&
                  artisan::IsCheckableInputType(*node)))) {
              activateControl(node, mouseInit);
            } else if (node != nullptr && node->tagName() == "a") {
              // Dispatches "click" first (previously an <a> never fired
              // any event at all - this both fixes that and gives
              // preventDefault() real teeth: a handler calling it
              // suppresses the navigation below, same as a real <a>.
              bool defaultPrevented = node->DispatchMouseEvent("click", mouseInit);
              if (!defaultPrevented) {
                const std::string *href = node->GetAttribute("href");
                if (href != nullptr) {
                  navigate(*href);
                }
              }
            } else if (node != nullptr && node->tagName() == "label") {
              const std::string *forId = node->GetAttribute("for");
              if (forId != nullptr) {
                activateControl(document->FindById(*forId), mouseInit);
              }
            }
          }
          needsRedraw = true;
        }
        break;

      case SDL_MOUSEMOTION: {
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

        // :hover tracking - boxRegions are in unscrolled content
        // coordinates, same adjustment the click handler below already
        // makes. A change here means a `:hover` rule might now apply (or
        // stop applying) somewhere, so it needs a redraw of its own,
        // independent of whatever isSelecting above just did.
        const BoxRegion *hoverRegion = HitTestRegion(
            boxRegions, static_cast<float>(event.motion.x),
            static_cast<float>(event.motion.y) + scrollY);
        Node *newHovered =
            hoverRegion != nullptr
                ? static_cast<Node *>(const_cast<void *>(hoverRegion->userData))
                : nullptr;
        if (newHovered != hoveredNode) {
          hoveredNode = newHovered;
          needsRedraw = true;
        }
        break;
      }

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

    // Checked once per iteration of this otherwise event-driven loop -
    // there's no separate timer thread/signal, so a due setTimeout/
    // setInterval only actually runs when the loop happens to come back
    // around to this point. The loop is uncapped (no SDL_Delay anywhere),
    // so in practice that's effectively immediately.
    if (timerQueue.FireDue(SDL_GetTicks())) {
      needsRedraw = true;
    }

    // requestAnimationFrame callbacks fire every iteration too, not just
    // when needsRedraw is already true for some other reason - real rAF
    // drives its own animation loop (a callback that calls
    // requestAnimationFrame again is the normal way to animate
    // continuously), it doesn't wait for an unrelated redraw to piggyback
    // on. SDL_RenderPresent below already runs every iteration regardless
    // (only the texture regen is conditional on needsRedraw), so "before
    // the next repaint" and "this iteration" are effectively the same
    // point here.
    if (animationFrameQueue.FireAll(static_cast<double>(SDL_GetTicks()))) {
      needsRedraw = true;
    }

    // Drains QuickJS's own job queue (every pending Promise .then()/
    // async-function continuation) once per iteration, after the above -
    // so a continuation queued by an SDL event handler, a due timer, or
    // an animation frame this same iteration gets a chance to run before
    // the render below, rather than sitting queued until some future
    // iteration. Not per-macrotask-precise (real engines drain
    // microtasks after *every single* callback, not once per batch) -
    // see JsEngine::PumpJobQueue's own doc comment for why that's an
    // accepted simplification here, not a bug.
    if (jsEngine && jsEngine->PumpJobQueue()) {
      needsRedraw = true;
    }

    if (needsRedraw) {
      SDL_Texture *newTexture = RenderFrame(
          renderer, measuringWidgetRenderer, *document, focus,
          artisan::PseudoClassState{hoveredNode, focus.node}, width, height,
          &boxRegions, &contentHeight);
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
