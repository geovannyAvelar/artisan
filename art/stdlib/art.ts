// The DOM bridge - Node/Event and the raw FFI surface (each a real C++
// symbol in artisan's own runtime, see art_bridge.h) they're built from.
// Part of ART's standard library, not any one project - `import { Node,
// Event } from "art";` pulls this in from anywhere, no relative path or
// per-project copy needed (see ModuleResolver::ResolveImportPath: a bare,
// non-relative import path resolves against this directory). A project
// otherwise never needs to look at this file - it's what a fresh `app.ts`
// starts free of, ~150 lines of `declare`s down to one `import` line.
//
// It's still ordinary ART source, though, not generated/off-limits, and
// exactly where you'd add a wrapper for more of include/node_c_api.h if
// you needed it (see README.md's "Using ART" section for how the
// `node.getElementById(id)` -> `ArtFindById(node, id)` desugaring works -
// it's pure call-site sugar, no vtable/dynamic dispatch involved).
// Editing it here affects every project built against this checkout, not
// just one - copy it into your own project instead (any name, e.g.
// `./dom.ts`) and `import` that relative path instead of `"art"` if you
// want changes scoped to a single app.
//
// `ArtDocument` is never called directly below - the bare identifier
// `document`, used anywhere in the project, is sugar for exactly that
// call (see README.md), the same way a real browser's own `document` is
// always just there, no lookup of your own required, and no import
// needed either: unlike everything else here, `ArtDocument` doesn't need
// to be `export`ed for that sugar to keep working from another file.
declare function ArtDocument(): Node;
declare function ArtFindById(root: Node, id: string): Node;
declare function ArtIsNull(node: Node): boolean;
declare function ArtGetTextContent(node: Node): string;
declare function ArtSetTextContent(node: Node, text: string): void;
declare function ArtGetAttribute(node: Node, name: string): string;
declare function ArtHasAttribute(node: Node, name: string): boolean;
declare function ArtSetAttribute(node: Node, name: string, value: string): void;
declare function ArtQuerySelector(root: Node, selector: string): Node;
declare function ArtChildCount(node: Node): number;
declare function ArtChildAt(node: Node, index: number): Node;
// Creates a detached node - not yet part of any tree until passed to
// ArtAppendChild/ArtInsertBefore below. A node created and never
// appended just leaks (this is a real C++-owned node, not ART's own
// GC-managed heap) - see art_bridge.h for the full ownership story.
declare function ArtCreateElement(tag: string): Node;
declare function ArtCreateTextNode(text: string): Node;
// Returns the appended/inserted node back, matching a real browser's own
// appendChild/insertBefore. Unlike a real browser, `before` must be an
// actual existing child, never null (ART has no null literal of its own
// to pass) - use appendChild for "insert at the end" instead of
// insertBefore(child, null).
declare function ArtAppendChild(parent: Node, child: Node): Node;
declare function ArtInsertBefore(parent: Node, child: Node, before: Node): Node;
declare function ArtRemoveChild(parent: Node, child: Node): Node;
declare function ArtRemove(node: Node): Node;
declare function ArtCloneNode(node: Node, deep: boolean): Node;
// The "class" attribute's space-separated tokens. ArtClassListToggle:
// `hasForce == false` toggles membership; `hasForce == true` pins it to
// `force` instead (real `classList.toggle(name, force)` semantics - ART
// has no optional parameters, so both booleans are always required).
declare function ArtClassListAdd(node: Node, name: string): void;
declare function ArtClassListRemove(node: Node, name: string): void;
declare function ArtClassListContains(node: Node, name: string): boolean;
declare function ArtClassListToggle(node: Node, name: string, hasForce: boolean, force: boolean): boolean;
// `node`'s inline `style="..."` attribute, one property at a time - only
// color/backgroundColor/fontWeight/borderColor/borderWidth are
// supported. "" if the property isn't set; an empty value removes it.
declare function ArtGetStyle(node: Node, property: string): string;
declare function ArtSetStyle(node: Node, property: string, value: string): void;
declare function ArtAddEventListener(node: Node, eventType: string, handler: (event: Event) => void, capture: boolean): void;
declare function ArtRemoveEventListener(node: Node, eventType: string, handler: (event: Event) => void, capture: boolean): void;
// Fires your own event (any type, not just built-in ones) at a node,
// carrying a typed `detail` payload - call as e.g.
// `ArtDispatchEvent::<number>(node, "scored", true, true, 10)`. T can be
// number, boolean, or string (the bridge provides a real, separately
// compiled ArtDispatchEvent$<T>/ArtEventDetail$<T> pair for each - an
// unsupported T, e.g. your own interface, is a link error, not a type
// error - see art_bridge.h). Only readable back with ArtEventDetail<T>
// in a listener on an event ART itself dispatched, and only with the
// *same* T (see its own doc comment for why). Returns false if the event
// was cancelable and some listener called event.preventDefault().
// Generic, so - unlike everything else here - it can't be wrapped as a
// class method yet (methods can't be generic - see README.md); called
// directly, e.g. `ArtDispatchEvent::<string>(document, "saved", true, true, "ok")`.
// `export`ed (unlike the rest of this raw FFI surface) since a project
// using custom events calls it directly, by name - add it (and
// ArtEventDetail<T> below) to your own `import { ... } from "art";` line.
export declare function ArtDispatchEvent<T>(node: Node, eventType: string, bubbles: boolean, cancelable: boolean, detail: T): boolean;
declare function ArtEventType(event: Event): string;
declare function ArtEventTarget(event: Event): Node;
// Same generic limitation as ArtDispatchEvent<T> above - called directly,
// e.g. `ArtEventDetail::<string>(event)`. Exported for the same reason.
export declare function ArtEventDetail<T>(event: Event): T;
declare function ArtEventBubbles(event: Event): boolean;
declare function ArtEventCancelable(event: Event): boolean;
declare function ArtEventPreventDefault(event: Event): void;
declare function ArtEventDefaultPrevented(event: Event): boolean;
declare function ArtEventStopPropagation(event: Event): void;
declare function ArtEventStopImmediatePropagation(event: Event): void;
declare function ArtEventClientX(event: Event): number;
declare function ArtEventClientY(event: Event): number;
declare function ArtEventCtrlKey(event: Event): boolean;
declare function ArtEventShiftKey(event: Event): boolean;
declare function ArtEventAltKey(event: Event): boolean;
declare function ArtEventMetaKey(event: Event): boolean;
declare function ArtEventKey(event: Event): string;
declare function ArtEventCode(event: Event): string;

// A DOM handle - opaque (no accessible fields, same as `declare type`),
// but its methods/properties are real ART code, each a thin wrapper over
// the matching Art* function above. `node.getElementById(id)` compiles
// to exactly `ArtFindById(node, id)` - no runtime cost, just nicer to
// read/write, and named/shaped to match a real browser's own DOM API:
// `get`/`set` accessors (see README.md's "Classes" section) wherever a
// browser would use a plain property (`node.textContent`, not
// `node.getTextContent()`), ordinary methods everywhere a browser has an
// actual method (`getElementById`, `setAttribute`, `addEventListener`,
// ...). `export`ed so a project can use it as a type - a method body's
// own calls into the raw Art* functions above resolve against this file,
// not the importing one, so none of those need exporting too.
export declare class Node {
  function getElementById(id: string): Node { return ArtFindById(this, id); }
  function isNull(): boolean { return ArtIsNull(this); }
  get textContent(): string { return ArtGetTextContent(this); }
  set textContent(text: string) { ArtSetTextContent(this, text); }
  function getAttribute(name: string): string { return ArtGetAttribute(this, name); }
  function hasAttribute(name: string): boolean { return ArtHasAttribute(this, name); }
  function setAttribute(name: string, value: string): void { ArtSetAttribute(this, name, value); }
  function querySelector(selector: string): Node { return ArtQuerySelector(this, selector); }
  function childCount(): number { return ArtChildCount(this); }
  function childAt(index: number): Node { return ArtChildAt(this, index); }
  // `this` is unused (ignored) here - matches document.createElement in a
  // real browser (a Document method, not really "of" any particular
  // node), but every ART method needs a receiver, so it's callable on
  // any Node, document included, e.g. `document.createElement("div")`.
  function createElement(tag: string): Node { return ArtCreateElement(tag); }
  function createTextNode(text: string): Node { return ArtCreateTextNode(text); }
  // Returns the appended/inserted node back, matching a real browser.
  // `before` must be an actual existing child of this node - use
  // appendChild for "insert at the end" instead of insertBefore(child,
  // null), which ART can't express (no null literal of its own).
  function appendChild(child: Node): Node { return ArtAppendChild(this, child); }
  function insertBefore(child: Node, before: Node): Node { return ArtInsertBefore(this, child, before); }
  function removeChild(child: Node): Node { return ArtRemoveChild(this, child); }
  // Detaches this node from its own parent directly - no need to look
  // the parent up first just to call removeChild on it.
  function remove(): Node { return ArtRemove(this); }
  function cloneNode(deep: boolean): Node { return ArtCloneNode(this, deep); }
  // The "class" attribute's space-separated tokens - flattened onto Node
  // directly (`node.classListAdd(...)`, not a nested `classList` object).
  function classListAdd(name: string): void { ArtClassListAdd(this, name); }
  function classListRemove(name: string): void { ArtClassListRemove(this, name); }
  function classListContains(name: string): boolean { return ArtClassListContains(this, name); }
  // Plain toggle: classListToggle(name, false, false). Forced membership
  // (real classList.toggle(name, force)): classListToggle(name, true, force).
  function classListToggle(name: string, hasForce: boolean, force: boolean): boolean {
    return ArtClassListToggle(this, name, hasForce, force);
  }
  // This node's inline `style="..."` attribute, one property at a time -
  // only color/backgroundColor/fontWeight/borderColor/borderWidth are
  // supported. "" if unset; setting "" removes the property.
  function getStyle(property: string): string { return ArtGetStyle(this, property); }
  function setStyle(property: string, value: string): void { ArtSetStyle(this, property, value); }
  // Stacks (multiple listeners on the same node/type all run) rather
  // than replacing, and the handler receives the Event itself - see the
  // Event class below for what you can read/call on it. This is the
  // only way to register a click (or any other) listener - there's no
  // separate "onclick"-style single-handler method, same as a real
  // browser's own addEventListener is the modern, preferred way to do
  // this over the legacy `el.onclick = fn` property.
  function addEventListener(eventType: string, handler: (event: Event) => void, capture: boolean): void {
    ArtAddEventListener(this, eventType, handler, capture);
  }
  // Removes a listener previously added with the exact same eventType/
  // handler/capture - a mismatched call (wrong handler, or one never
  // added) is a safe no-op.
  function removeEventListener(eventType: string, handler: (event: Event) => void, capture: boolean): void {
    ArtRemoveEventListener(this, eventType, handler, capture);
  }
}

export declare class Event {
  // Named eventType, not type - 'type' is a reserved word (used by
  // 'declare type'), so even as a property it can't be spelled the way a
  // real browser's own Event.type is.
  get eventType(): string { return ArtEventType(this); }
  get target(): Node { return ArtEventTarget(this); }
  get bubbles(): boolean { return ArtEventBubbles(this); }
  get cancelable(): boolean { return ArtEventCancelable(this); }
  // preventDefault/stopPropagation/stopImmediatePropagation stay ordinary
  // methods, not properties - they're actions, matching how a real
  // browser's own Event has them too (only its read-only data is
  // exposed as properties).
  function preventDefault(): void { ArtEventPreventDefault(this); }
  get defaultPrevented(): boolean { return ArtEventDefaultPrevented(this); }
  function stopPropagation(): void { ArtEventStopPropagation(this); }
  function stopImmediatePropagation(): void { ArtEventStopImmediatePropagation(this); }
  get clientX(): number { return ArtEventClientX(this); }
  get clientY(): number { return ArtEventClientY(this); }
  get ctrlKey(): boolean { return ArtEventCtrlKey(this); }
  get shiftKey(): boolean { return ArtEventShiftKey(this); }
  get altKey(): boolean { return ArtEventAltKey(this); }
  get metaKey(): boolean { return ArtEventMetaKey(this); }
  get key(): string { return ArtEventKey(this); }
  get code(): string { return ArtEventCode(this); }
}
