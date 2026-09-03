#pragma once

#include <cstdint>

// A small, hand-picked subset of node_c_api.h's DOM API, re-exposed under
// ART-ABI-compatible signatures (see art/ - the ART compiler) for an
// app.ts file's `declare function` imports to bind to directly.
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

// Creates a detached element/text node - not yet part of any tree until
// passed to ArtAppendChild/ArtInsertBefore below. See
// ArtisanCreateElement/ArtisanCreateTextNode's own doc comment for the
// ownership model this inherits as-is: alive but leaked (not ART's own
// GC-managed heap - this is a real, C++-owned artisan::Node) if created
// and never appended - the same accepted tradeoff the existing Go/JS
// bindings already have, not something ART's own GC involves itself in.
void *ArtCreateElement(ArtString *tag);
void *ArtCreateTextNode(ArtString *text);

// Appends/inserts `child` (from ArtCreateElement/ArtCreateTextNode above,
// or detached via ArtRemove/ArtRemoveChild below) into `parent`'s
// children - see ArtisanNodeAppendChild/ArtisanNodeInsertBefore. Returns
// `child` back, matching real DOM's own appendChild/insertBefore return
// value. Unlike real DOM's insertBefore, `before` here must be an actual
// existing child, never null (ART has no null literal of its own to
// pass) - use ArtAppendChild for "insert at the end" instead of
// insertBefore(child, null).
void *ArtAppendChild(void *parent, void *child);
void *ArtInsertBefore(void *parent, void *child, void *before);

// Detaches `child`/this node from its parent, re-registering it as a
// pending node (same state ArtCreateElement's own result starts in -
// alive but leaked unless re-appended). nullptr (see ArtIsNull) if
// `child` isn't actually a child of `parent` (ArtRemoveChild), or if
// `node` has no parent (ArtRemove). See Node::RemoveChild/Remove.
void *ArtRemoveChild(void *parent, void *child);
void *ArtRemove(void *node);

// A new, detached, pending node (same ownership state as
// ArtCreateElement's result) - same tag/text/attributes as `node`, never
// event listeners. `deep`: also clones every descendant. See
// Node::CloneNode.
void *ArtCloneNode(void *node, bool deep);

// The "class" attribute's space-separated tokens - see
// ArtisanNodeClassListAdd/Remove/Contains/Toggle (node_c_api.h). Flattened
// onto Node directly (`node.classListAdd(...)`, not a separate
// `classList` object) - there's no separate handle for it at the C API
// level either, just these four operations against the node's own
// "class" attribute. `ArtClassListToggle`: `hasForce == false` toggles
// membership; `hasForce == true` pins it to `force` instead (real
// `classList.toggle(name, force)` semantics - ART has no optional
// parameters to make `force` truly optional, so both booleans are always
// required, same convention `ArtAddEventListener`'s `capture` already
// has). Returns the resulting membership (true = now present).
void ArtClassListAdd(void *node, ArtString *name);
void ArtClassListRemove(void *node, ArtString *name);
bool ArtClassListContains(void *node, ArtString *name);
bool ArtClassListToggle(void *node, ArtString *name, bool hasForce, bool force);

// `node`'s inline `style="..."` attribute, one property at a time - only
// the five properties a `<style>` block supports here (color/
// backgroundColor/fontWeight/borderColor/borderWidth - see css.h). ""
// (not null - ART's string type can't represent that, same convention
// ArtGetAttribute already has) if the property isn't set. An empty
// `value` passed to ArtSetStyle removes the property, matching
// ArtisanNodeStyleSet.
ArtString *ArtGetStyle(void *node, ArtString *property);
void ArtSetStyle(void *node, ArtString *property, ArtString *value);

// A void-returning ART function, passed as a code address PLUS a
// captured environment (see art/codegen.cpp's Handler-related codegen -
// ART's only function-pointer-shaped value, written `(params...) =>
// void`). Every Handler value - a plain top-level function reference or
// a real closure alike - is callable through this exact same uniform
// "(env, ...params) -> void" convention: `env` is simply null for a
// plain function reference (see Codegen::GetOrCreatePlainThunk), or a
// pointer to a GC-allocated struct of captured-variable cell addresses
// for a real closure (see Codegen::GenClosureFunction) - opaque to this
// side either way, just threaded through and handed back on the next
// call. No registration/release bookkeeping needed the way Go's
// cgo.Handle scheme required (contrast ArtisanNodeSetOnClick's
// `uintptr_t handle` in node_c_api.h) - `env`, like `handler` itself, is
// just a plain pointer with no separate lifetime to manage; it's kept
// alive by the same GC that keeps everything else ART allocates alive.
using ArtHandler = void (*)(void *env);                     // "() => void" - see ArtSetOnClick
using ArtEventHandler = void (*)(void *env, void *event);   // "(event: Event) => void" - see ArtAddEventListener

// See Node::SetOnClick - replaces any handler already registered on this
// node, the same way Node::SetOnClick's std::function replacement always
// does.
void ArtSetOnClick(void *node, ArtHandler handler, void *env);

// See Node::AddEventListener - stacks alongside, never replaces, any
// other listener already registered for `eventType` (or any other type),
// same as the real method. `handler` receives an opaque `Event` handle -
// a foreign type ART only ever holds and passes to the ArtEvent*
// functions below, same relationship it has with `Node` itself.
void ArtAddEventListener(void *node, ArtString *eventType, ArtEventHandler handler, void *env, bool capture);

// See Node::RemoveEventListener - removes every listener registered for
// `eventType`/`capture` on this node whose handler/env is `handler`/`env`
// itself (recovered via std::function::target<T>() on the
// art_bridge.cpp-only wrapper class ArtAddEventListener actually stored
// - see there). Both must match, not just `handler` - two closures
// compiled from the same source literal (e.g. two different loop
// iterations) share one generated thunk function pointer but have
// different envs, so matching on `handler` alone could remove the wrong
// one. A `handler`/`env` pair that matches nothing registered is a safe
// no-op, same as the real method.
void ArtRemoveEventListener(void *node, ArtString *eventType, ArtEventHandler handler, void *env, bool capture);

// See Node::DispatchEvent - fires `eventType` at `node` through the usual
// capturing/target/bubbling walk, running every listener registered for
// it (ArtAddEventListener/ArtSetOnClick alike), Go- or JS-registered ones
// included, same as any internally-fired click/change/input. Returns
// false if the event was cancelable and some listener called
// ArtEventPreventDefault, true otherwise - the real method's own
// convention.
//
// `detail`'s type is ART's own generic type parameter T - ART source
// declares this (and ArtEventDetail below) as:
//
//   declare function ArtDispatchEvent<T>(node: Node, eventType: string,
//       bubbles: boolean, cancelable: boolean, detail: T): boolean;
//   declare function ArtEventDetail<T>(event: Event): T;
//
// and each call site instantiates T explicitly, e.g.
// `ArtDispatchEvent::<number>(...)` - ART's compiler mangles that
// instantiation's actual C symbol to `ArtDispatchEvent$number`, so this
// header/its .cpp only ever need to provide the *concrete* overloads
// below, one pair per T actually usable this way (`number`/`boolean`/
// `string` - the primitive types with an obvious, safe heap
// representation to box `detail` as; there's no generic mangled overload
// for an arbitrary user interface T, since a finite bridge can't provide
// one for every interface a project might declare). Calling either with
// a T this header doesn't provide an overload for is a link error
// (undefined symbol) - the honest outcome, the same as an unprovided
// explicit template instantiation would be across a real C++ ABI
// boundary.
//
// `Event::detail` is still a bare `const void*` underneath (see
// dom_node.h) - even with T known at the ART call site, `detail` is only
// safe to read back with ArtEventDetail<T> in a listener on an event
// *ART itself* dispatched, and only with the *same* T: nothing tags the
// heap box with which T it actually holds, so a listener has no way to
// know a JS/Go dispatch's own detail (there, typically a boxed script
// value) isn't one of these ART-shaped boxes, or that an
// ArtEventDetail::<string> call against a box `ArtDispatchEvent::<number>`
// made isn't reading four bytes into a `double` as if it were an
// `ArtString*`. This is the exact same "each binding (and, now, each T)
// owns its own interpretation" contract `detail` always had.
bool ArtDispatchEvent$number(void *node, ArtString *eventType, bool bubbles, bool cancelable, double detail);
bool ArtDispatchEvent$boolean(void *node, ArtString *eventType, bool bubbles, bool cancelable, bool detail);
bool ArtDispatchEvent$string(void *node, ArtString *eventType, bool bubbles, bool cancelable, ArtString *detail);
double ArtEventDetail$number(void *event);
bool ArtEventDetail$boolean(void *event);
ArtString *ArtEventDetail$string(void *event);

// See Event (dom_node.h) for the real fields these read/call.
ArtString *ArtEventType(void *event);
void *ArtEventTarget(void *event); // a Node - see Event::target
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

// setTimeout/setInterval/requestAnimationFrame - see TimerQueue/
// AnimationFrameQueue (timer_queue.h/animation_frame_queue.h), scheduled
// into via art_bridge_context.h's SetArtTimerContext (main.cpp calls it
// once per navigate(), same place SetArtDocumentContext already is). A
// no-op returning 0 - an id no real timer/frame ever has, both queues'
// ids start at 1 - if called before a page is ready to schedule into,
// same tolerance ArtDocument()/ArtIsNull already has for "no document
// yet"; this should never actually happen from within a real ART
// program's own top-level code or a handler. ArtClearTimer cancels
// either a setTimeout or a setInterval id - the same single entry point
// real clearTimeout/clearInterval each are.
//
// Every id here is `double`, not `int`, despite TimerQueue/
// AnimationFrameQueue using plain `int` internally: ART's `number` type
// is always a C ABI `double` (see art/codegen.cpp's MapType), so a
// `declare function` returning/taking `number` needs an actual `double`
// on this side of the boundary too, or the mismatched calling
// convention (an integer register vs. an XMM one) hands back/reads
// garbage - caught by a real ArtClearTimer(id) call silently failing to
// cancel anything, id truncation/precision loss aside (never a real
// concern here: every id fits exactly in a double already).
using ArtAnimationFrameHandler = void (*)(void *env, double timestamp); // "(timestamp: number) => void"
double ArtSetTimeout(ArtHandler callback, void *env, double delayMs);
double ArtSetInterval(ArtHandler callback, void *env, double delayMs);
void ArtClearTimer(double id);
double ArtRequestAnimationFrame(ArtAnimationFrameHandler callback, void *env);
void ArtCancelAnimationFrame(double id);

} // extern "C"
