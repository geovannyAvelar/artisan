# ART Language Guide

Complete guide to the ART language and its features.

## Overview

ART is a statically-typed, TypeScript-like language compiled ahead-of-time to native machine code via LLVM. It has TypeScript syntax but removes dynamic features (no prototypes, no dynamic property access, no instanceof/downcasting).

**Key characteristics:**
- **Compiled** - Native machine code via LLVM
- **Statically typed** - Type checked at compile time
- **TypeScript-like** - Familiar syntax for TS developers
- **Fast** - No runtime overhead, runs at native speed
- **Safe** - Type system prevents many common bugs

## Getting Started with ART

### Your First ART App

Create `app.tsx`:

```typescript
import { Node, Event } from "art";

function onButtonClick(event: Event): void {
  let button: Node = event.target;
  button.textContent = "Clicked!";
}

let button: Node = document.getElementById("my-button");
if (!button.isNull()) {
  button.addEventListener("click", onButtonClick, false);
  button.textContent = "Click me";
}
```

Create `pages/index.html`:

```html
<div id="root">
  <button id="my-button">Click me</button>
</div>
```

Build and run:

```bash
artisan-cli build my-app --run
```

### Implicit vs Explicit Setup

ART automatically wraps top-level code in a `setupApp()` function:

```typescript
// Implicit - top-level code
let button: Node = document.getElementById("my-button");
button.textContent = "Ready";
```

Or explicitly:

```typescript
function setupApp(): void {
  let button: Node = document.getElementById("my-button");
  button.textContent = "Ready";
}
```

Both are equivalent. `setupApp` runs once per page load.

## Language Features

### Variables and Types

```typescript
// Type declarations
let count: number = 0;
let name: string = "John";
let active: boolean = true;

// Const (immutable)
const PI: number = 3.14;

// Inferred types
let inferred = 42;  // number inferred
```

### Functions

```typescript
// Function declaration
function add(a: number, b: number): number {
  return a + b;
}

// Function pointer type
let callback: (event: Event) => void;

// Closures (capturing outer variables)
function makeCounter(start: number): () => number {
  let count: number = start;
  return function(): number {
    count = count + 1;
    return count;
  };
}
```

### Interfaces

```typescript
interface Person {
  name: string;
  age: number;
  greet(): string;
}

let person: Person = {
  name: "Alice",
  age: 30,
  greet: function(): string {
    return "Hello, " + this.name;
  }
};
```

### Generic Functions

```typescript
// Generic function - must use turbofish syntax at call site
function identity<T>(x: T): T {
  return x;
}

// Must specify type explicitly
let num: number = identity::<number>(42);
let str: string = identity::<string>("hello");
```

### Generic Interfaces

```typescript
interface Box<T> {
  value: T;
  getValue(): T;
}

let numberBox: Box<number> = {
  value: 42,
  getValue: function(): number {
    return this.value;
  }
};
```

### Control Flow

```typescript
// If/else
if (count > 0) {
  // ...
} else if (count == 0) {
  // ...
} else {
  // ...
}

// While loop
while (i < 10) {
  // ...
  i = i + 1;
}

// For loop (C-style)
for (let i: number = 0; i < 10; i = i + 1) {
  // ...
}

// Switch
switch (value) {
  case 1:
    // ...
    break;
  case 2:
    // ...
    break;
  default:
    // ...
}
```

### Type Narrowing

Use `typeof` to narrow types:

```typescript
function process(value: any): void {
  if (typeof value == "string") {
    // value is string here
    console.log(value.length);
  } else if (typeof value == "number") {
    // value is number here
    console.log(value + 1);
  }
}
```

## DOM API

The DOM is your main way to interact with the UI. All DOM types come from `"art"`:

```typescript
import { Node, Event } from "art";
```

### Document

```typescript
// Get the document (ambient global - always available)
let doc: Node = document;

// Find elements
let element: Node = document.getElementById("my-id");
let first: Node = document.querySelector(".my-class");

// Create elements
let div: Node = document.createElement("div");
let text: Node = document.createTextNode("Hello");
```

### Node Properties

```typescript
// Read/write text content
node.textContent = "Hello";
let text: string = node.textContent;

// Check if null (no null literal in ART)
if (!node.isNull()) {
  // Node is valid
}

// Get tag name
let tag: string = node.tagName;  // "div", "button", etc.
```

### Attributes

```typescript
// Get/set attributes
node.setAttribute("data-id", "123");
let id: string = node.getAttribute("data-id");  // "123"

// Check if attribute exists
if (node.hasAttribute("disabled")) {
  // ...
}

// Remove attribute
node.removeAttribute("disabled");
```

### Classes

```typescript
// Manage CSS classes
node.classListAdd("active");
node.classListRemove("inactive");

let isActive: boolean = node.classListContains("active");

// Toggle class
let newState: boolean = node.classListToggle("selected", false, false);

// Toggle with force
node.classListToggle("disabled", true, true);  // Force add
node.classListToggle("disabled", true, false); // Force remove
```

### Styles

```typescript
// Set inline styles
node.setStyle("color", "blue");
node.setStyle("backgroundColor", "red");
node.setStyle("fontWeight", "bold");

// Get inline style
let color: string = node.getStyle("color");

// Remove style (set to empty string)
node.setStyle("color", "");
```

Supported style properties:
- `color`, `backgroundColor`
- `fontWeight`
- `borderColor`, `borderWidth`

### Tree Manipulation

```typescript
// Get children
let count: number = node.childCount();
let child: Node = node.childAt(0);

// Add/insert children
node.appendChild(newChild);
node.insertBefore(newChild, referenceChild);

// Remove children
node.removeChild(child);

// Remove self
node.remove();

// Clone
let copy: Node = node.cloneNode(true);  // deep = true
```

### Traversal

```typescript
// Get parent
let parent: Node = node.parentNode;

// Get siblings
let next: Node = node.nextSibling;
let prev: Node = node.previousSibling;

// Get owner document
let doc: Node = node.ownerDocument;
```

### Querying

```typescript
// Find first match
let first: Node = node.querySelector(".item");

// Find all matches (snapshot array)
let all: Node[] = node.querySelectorAll(".item");

// Test if matches selector
let matches: boolean = node.matches(".item");

// Find closest ancestor
let container: Node = node.closest(".container");
```

## Event Handling

### Adding Listeners

```typescript
function onClick(event: Event): void {
  let target: Node = event.target;
  // Handle click
}

node.addEventListener("click", onClick, false);

// Capture phase
node.addEventListener("click", onClick, true);
```

### Event Properties

```typescript
function handleEvent(event: Event): void {
  // Event type
  let type: string = event.eventType;  // "click", "keydown", etc.
  
  // Event target
  let target: Node = event.target;
  
  // Bubble control
  let bubbles: boolean = event.bubbles;
  let cancelable: boolean = event.cancelable;
  
  // Prevent default
  event.preventDefault();
  let prevented: boolean = event.defaultPrevented;
  
  // Stop propagation
  event.stopPropagation();
  event.stopImmediatePropagation();
}
```

### Mouse Events

```typescript
function handleMouseEvent(event: Event): void {
  let x: number = event.clientX;
  let y: number = event.clientY;
  
  let ctrl: boolean = event.ctrlKey;
  let shift: boolean = event.shiftKey;
  let alt: boolean = event.altKey;
  let meta: boolean = event.metaKey;
}
```

### Keyboard Events

```typescript
function handleKeyEvent(event: Event): void {
  let key: string = event.key;      // "a", "Enter", etc.
  let code: string = event.code;    // "KeyA", "Enter", etc.
  
  let ctrl: boolean = event.ctrlKey;
  let shift: boolean = event.shiftKey;
  let alt: boolean = event.altKey;
}
```

### Removing Listeners

```typescript
// Must be the exact same function reference
node.removeEventListener("click", onClick, false);
```

### Dispatching Events

```typescript
// Dispatch custom event
let prevented: boolean = ArtDispatchEvent::<number>(
  node,
  "custom-event",
  true,   // bubbles
  true,   // cancelable
  42      // detail value
);

// In a listener on that event:
function handleCustom(event: Event): void {
  let value: number = ArtEventDetail::<number>(event);
}
```

## Timers

```typescript
// setTimeout
let id: number = setTimeout(function(): void {
  // Runs after delay
}, 1000);

// Cancel
clearTimeout(id);

// setInterval
let intervalId: number = setInterval(function(): void {
  // Runs every 1000ms
}, 1000);

// Cancel
clearInterval(intervalId);

// requestAnimationFrame
let frameId: number = requestAnimationFrame(function(timestamp: number): void {
  // Runs before next frame
});

// Cancel
cancelAnimationFrame(frameId);
```

## JSX

ART supports JSX syntax:

```typescript
// JSX expression
let element: Node = <div className="container">
  <h1>Hello</h1>
  <p>World</p>
</div>;

// Function components
function Button(): Node {
  return <button>Click me</button>;
}

// With props (as attributes)
let widget: Node = <Button color="blue" />;
```

## Modules

Import and export types and functions:

```typescript
// Export
export function helper(): string {
  return "help";
}

export interface Config {
  name: string;
}

// Import
import { helper, Config } from "./lib";

let h: string = helper();
let cfg: Config = { name: "app" };
```

## Checked `any` Type

When you need dynamic typing:

```typescript
function processData(data: any): void {
  // Must check type before using
  if (typeof data == "string") {
    console.log(data.length);  // Now narrowed to string
  } else if (typeof data == "number") {
    console.log(data + 1);     // Now narrowed to number
  }
}
```

## Performance Tips

1. **Avoid unnecessary DOM queries** - Cache node references
2. **Batch DOM updates** - Group modifications together
3. **Use event delegation** - One listener on parent vs many on children
4. **Clean up listeners** - Remove listeners when done
5. **Keep functions simple** - Easier for optimizer to inline

## Common Patterns

### Component-like Pattern

```typescript
interface Component {
  render(): Node;
  update(data: any): void;
  destroy(): void;
}

class Button {
  private element: Node;
  
  render(): Node {
    this.element = document.createElement("button");
    this.element.textContent = "Click";
    this.element.addEventListener("click", 
      function(e: Event): void { }, false);
    return this.element;
  }
  
  update(text: string): void {
    this.element.textContent = text;
  }
  
  destroy(): void {
    // Clean up
  }
}
```

### State Management

```typescript
interface State {
  count: number;
  name: string;
}

let state: State = {
  count: 0,
  name: "app"
};

function updateState(newState: State): void {
  state = newState;
  render();  // Re-render when state changes
}

function render(): void {
  // Update DOM based on state
}
```

## Debugging

### Console Output

```typescript
// Print to console
console.log("Message");
console.log("Value: " + value);
console.warn("Warning");
console.error("Error");
```

### Build with Debug Info

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Run with Debugger

```bash
gdb ./build/my-app
```

## Common Issues

### "Cannot access document in global initializer"

Document is only available during `setupApp`. Move code into setupApp:

```typescript
// ✗ Wrong
let button: Node = document.getElementById("my-button");

// ✓ Correct
function setupApp(): void {
  let button: Node = document.getElementById("my-button");
}
```

### "Node is null" errors

Always check if a node is null:

```typescript
let button: Node = document.getElementById("my-button");
if (!button.isNull()) {
  // Safe to use button
  button.textContent = "Ready";
}
```

### "Type mismatch" errors

Ensure variables match their declared type:

```typescript
let count: number = "42";  // ✗ Error: string can't assign to number
let count: number = 42;     // ✓ Correct
```

---

Ready to build something? Create a project and start coding!

See also:
- [Using JavaScript & React](JAVASCRIPT_GUIDE.md)
- [Project Structure](PROJECT_STRUCTURE.md)
