#pragma once

#include <cstdint>

// A small, hand-picked subset of node_c_api.h's DOM API, re-exposed under
// ART-ABI-compatible signatures (see art/ - the ART compiler) for an
// app.art file's `declare function` imports to bind to directly.
//
// ART's opaque `Node` type is a plain `void*` (its own compiled code never
// dereferences it, only ever passes it back into these functions) - the
// same ArtisanNode* handle node_c_api.h already uses, so it's passed
// through unchanged.
//
// ART's `string` type is a pointer to this exact header shape - a length
// plus a null-terminated (see art/codegen.cpp) byte buffer, so ArtString
// doubles as both "the type an extern function reads an ART string
// argument through" and "the type it returns a fresh ART string in".
// `data` is only ever read here, never retained past the call, matching
// node_c_api.h's own `const char*` ownership convention.
extern "C" {

struct ArtString {
  int64_t length;
  const char *data;
};

// The root Node of whichever page is currently loaded - the same one
// setupApp's own `document` parameter is, so a handler invoked later
// (with no parameters of its own - see ArtSetOnClick below) can still
// reach the DOM by calling this instead of needing setupApp to have
// stashed a Node somewhere first (which it can't: only number/boolean/
// string can be a top-level `let`/`const` - see README.md's "Using ART"
// section). nullptr (see ArtIsNull) in the brief window between one
// page's Node tree being torn down and the next one finishing its build
// - see art_bridge_context.h's SetArtDocumentContext, called by main.cpp
// once per navigate(), same place node_c_api_bridge.h's
// SetGoTimerContext is.
void *ArtDocument();

// nullptr (see ArtIsNull below - ART itself has no null literal to test
// this against directly) if no match. See ArtisanNodeFindById.
void *ArtFindById(void *root, ArtString *id);

// See ArtisanQuerySelector - same bounded selector grammar (css.h).
void *ArtQuerySelector(void *root, ArtString *selector);

// True if `node` is a null handle - the only way ART code can test a
// Node result (ArtFindById/ArtQuerySelector) for "no match", since ART
// itself has no null literal or equality against one.
bool ArtIsNull(void *node);

// See Node::textContent/SetTextContent.
ArtString *ArtGetTextContent(void *node);
void ArtSetTextContent(void *node, ArtString *text);

// "" (not null - ART's string type can't represent that) if the
// attribute isn't set. See Node::GetAttribute.
ArtString *ArtGetAttribute(void *node, ArtString *name);
bool ArtHasAttribute(void *node, ArtString *name);
void ArtSetAttribute(void *node, ArtString *name, ArtString *value);

// See Node::children. ArtChildAt is undefined behavior if `index` isn't
// `< ArtChildCount(node)`, same contract ArtisanNodeChildAt has.
double ArtChildCount(void *node);
void *ArtChildAt(void *node, double index);

// A void-returning ART function, passed as a plain code address (see
// art/codegen.cpp's ExprKind::Identifier handling for a bare function-
// name reference - ART's only function-pointer-shaped value, written
// `(params...) => void`). ART has no closures, so there's no captured
// environment to carry and no registration/release bookkeeping needed
// the way Go's cgo.Handle scheme requires - contrast
// ArtisanNodeSetOnClick's `uintptr_t handle` in node_c_api.h.
using ArtHandler = void (*)();             // "() => void" - see ArtSetOnClick
using ArtEventHandler = void (*)(void *);  // "(event: Event) => void" - see ArtAddEventListener

// See Node::SetOnClick - replaces any handler already registered on this
// node, the same way Node::SetOnClick's std::function replacement always
// does.
void ArtSetOnClick(void *node, ArtHandler handler);

// See Node::AddEventListener - stacks alongside, never replaces, any
// other listener already registered for `eventType` (or any other type),
// same as the real method. `handler` receives an opaque `Event` handle -
// a foreign type ART only ever holds and passes to the ArtEvent*
// functions below, same relationship it has with `Node` itself.
void ArtAddEventListener(void *node, ArtString *eventType, ArtEventHandler handler, bool capture);

// See Node::RemoveEventListener - removes every listener registered for
// `eventType`/`capture` on this node whose handler is `handler` itself
// (recovered via std::function::target<T>() on the art_bridge.cpp-only
// wrapper class ArtAddEventListener actually stored - see there). A
// `handler` that matches nothing registered is a safe no-op, same as the
// real method.
void ArtRemoveEventListener(void *node, ArtString *eventType, ArtEventHandler handler, bool capture);

// See Node::DispatchEvent - fires `eventType` at `node` through the usual
// capturing/target/bubbling walk, running every listener registered for
// it (ArtAddEventListener/ArtSetOnClick alike), Go- or JS-registered ones
// included, same as any internally-fired click/change/input. Returns
// false if the event was cancelable and some listener called
// ArtEventPreventDefault, true otherwise - the real method's own
// convention. `detail` is carried onto the dispatched Event completely
// unexamined (see Event::detail in dom_node.h - true even for a plain
// C++ `const void*`, let alone this) - pass "" for none. It's only safe
// to read back with ArtEventDetail below in a listener on an event *ART
// itself* dispatched: the underlying field is a bare `const void*` with
// no type tag, so a listener has no way to know a JS/Go dispatch's own
// detail (there, typically a boxed script value) isn't ART's string
// shape - this is the exact same "each binding owns its own
// interpretation" contract detail always had, just spelled out for ART.
bool ArtDispatchEvent(void *node, ArtString *eventType, bool bubbles, bool cancelable, ArtString *detail);

// See Event (dom_node.h) for the real fields these read/call.
ArtString *ArtEventType(void *event);
void *ArtEventTarget(void *event); // a Node - see Event::target
// The `detail` ArtDispatchEvent passed above, verbatim - "" if this
// event has none (a plain internally-fired click/change/input, or an
// ArtDispatchEvent call with detail ""). See ArtDispatchEvent's own doc
// comment for why this is only safe to call on an event ART itself
// dispatched.
ArtString *ArtEventDetail(void *event);
bool ArtEventBubbles(void *event);
bool ArtEventCancelable(void *event);
void ArtEventPreventDefault(void *event);   // no-op if !ArtEventCancelable(event), matching real DOM
bool ArtEventDefaultPrevented(void *event);
void ArtEventStopPropagation(void *event);
// Also implies ArtEventStopPropagation - stopping "immediately" means
// neither any remaining listener at the current node nor any further
// ancestor gets a turn, same as real DOM. No corresponding getter (no
// ArtEventPropagationStopped/ArtEventImmediatePropagationStopped) -
// matching real DOM's own API, which doesn't expose "was propagation
// already stopped" back to script either, only the action.
void ArtEventStopImmediatePropagation(void *event);
// MouseEvent/KeyboardEvent data - 0/false/"" unless the event that fired
// is actually one of those (see Event's own doc comment in dom_node.h).
double ArtEventClientX(void *event);
double ArtEventClientY(void *event);
bool ArtEventCtrlKey(void *event);
bool ArtEventShiftKey(void *event);
bool ArtEventAltKey(void *event);
bool ArtEventMetaKey(void *event);
ArtString *ArtEventKey(void *event);
ArtString *ArtEventCode(void *event);

} // extern "C"
