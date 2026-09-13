# React Integration Guide for Artisan

Artisan includes full support for running **React 18.3.1** applications through its embedded QuickJS JavaScript engine. This guide explains how to set up and use React in your Artisan projects.

## Overview

The Artisan framework provides:
- **Vendored React & ReactDOM** - React 18.3.1 UMD builds in `third_party/react/`
- **DOM API** - Full DOM bindings through QuickJS (document, elements, events, styles)
- **JSX Compilation** - Automatic JSX → JavaScript transformation
- **ART Stdlib** - Comprehensive TypeScript standard library including React integration utilities

## Quick Start

### 1. Create a React App

Create an `app.jsx` file in your Artisan project:

```jsx
import { useState } from "react";

function Counter() {
  const [count, setCount] = useState(0);
  
  return (
    <div>
      <p>Count: {count}</p>
      <button onclick={() => setCount(count + 1)}>Increment</button>
    </div>
  );
}

// Mount to DOM
const root = document.getElementById("root");
if (root) {
  const container = document.createElement("div");
  root.appendChild(container);
  
  // ReactDOM.createRoot(container).render(<Counter />);
  // Note: Actual React import mechanism depends on js-prelude.js setup
}
```

### 2. Set Up React Runtime

The React runtime can be loaded via `js-prelude.js`, which is embedded before `app.js`/`app.jsx`:

```javascript
// js-prelude.js - Loaded before app code

// Concatenate react.development.js and react-dom.development.js from third_party/react/
// Then reassign the h and Fragment globals:

globalThis.h = React.createElement;
globalThis.Fragment = React.Fragment;

// Expose ReactDOM if needed
globalThis.ReactDOM = window.ReactDOM;
```

Once React is loaded this way:
- All `.jsx` files automatically use React via the JSX transform
- `React.useState`, `React.useEffect`, etc. are available
- `ReactDOM.createRoot()` works as normal

### 3. Build and Run

```bash
artisan build
artisan run
```

The QuickJS engine will:
1. Load React (via js-prelude.js)
2. Evaluate your app.jsx (compiled to JS)
3. Render React components to the DOM

## Architecture

### Component Layers

```
┌─────────────────────────────┐
│   React Components (JSX)    │
│  (app.jsx, React.useState)  │
└──────────────┬──────────────┘
               │ JSX Transform
┌──────────────▼──────────────┐
│   JavaScript (app.js)       │
│  (h(), Fragment calls)      │
└──────────────┬──────────────┘
               │ QuickJS Engine
┌──────────────▼──────────────┐
│  DOM API Bindings           │
│ (document, elements, etc.)  │
└──────────────┬──────────────┘
               │ C++
┌──────────────▼──────────────┐
│   Artisan Node Tree         │
│  (Real DOM structure)       │
└─────────────────────────────┘
```

### Key Files

- **third_party/react/** - Vendored React 18.3.1 UMD builds
  - `react.development.js` - React core
  - `react-dom.development.js` - ReactDOM for rendering

- **art/stdlib/react.ts** - ART-level React integration utilities
  - Component management
  - Root creation and rendering
  - Hooks API wrappers

- **include/js_engine.h** - QuickJS integration with DOM bindings
  - `document` API
  - `Node` methods and properties
  - Event handling
  - Timer functions

## Using the ART React Module

For advanced integration from ART code, use the `art/react` module:

```typescript
import { createReactRoot, renderToRoot, clearReactContexts } from "art/react";

function setupReactApp(rootElement: any, AppComponent: any): void {
  // Create a React root
  let context = createReactRoot(rootElement, AppComponent);
  
  // Render the component
  renderToRoot(context, AppComponent);
}
```

## DOM API Reference

The QuickJS engine exposes a DOM-like API:

### Document Methods
```javascript
document.getElementById(id)
document.querySelector(selector)
document.querySelectorAll(selector)
document.createElement(tagName)
document.createTextNode(text)
```

### Element Properties & Methods
```javascript
element.tagName                    // Read-only
element.textContent                // Read/write
element.getAttribute(name)
element.setAttribute(name, value)
element.classList.add/remove/toggle(name)
element.style.color = "red"        // CSS properties
element.appendChild(child)
element.insertBefore(child, ref)
element.removeChild(child)
element.addEventListener(type, fn)
element.dispatchEvent(event)
```

### Event Handling
```javascript
element.addEventListener("click", function(event) {
  console.log(event.type, event.target);
  event.preventDefault();
});

// Mouse events have clientX, clientY, ctrlKey, shiftKey, metaKey
// Keyboard events have key, code

const event = new Event("custom", { bubbles: true });
element.dispatchEvent(event);
```

## React Hooks

All React Hooks work as normal when React is loaded:

```javascript
// State
const [count, setCount] = useState(0);

// Effects
useEffect(() => {
  console.log("Component mounted");
  return () => console.log("Component unmounted");
}, []);

// Callbacks and Memoization
const handleClick = useCallback(() => setCount(c => c + 1), []);
const memoized = useMemo(() => computeValue(), [dependency]);

// Context
const theme = useContext(ThemeContext);

// Refs
const inputRef = useRef(null);
```

## Event Handling

React events use real DOM events. Note the API differences:

```jsx
// Artisan/Real DOM (lowercase event names)
<button onclick={() => doSomething()}>Click me</button>
<input onchange={(e) => handleChange(e)} />

// Not React's camelCase
// <button onClick={() => doSomething()}>Click me</button>
```

## Styling

CSS can be applied through:

```jsx
// Inline styles (camelCase CSS properties map to DOM)
<div style={{ color: "red", backgroundColor: "blue" }}>
  Styled text
</div>

// className attribute with CSS
<div class="my-class">Content</div>

// Direct style manipulation
element.style.color = "red";
element.style.backgroundColor = "blue";
element.classList.add("active");
```

## Common Patterns

### Functional Components
```javascript
function MyComponent({ name, children }) {
  return (
    <>
      <h1>{name}</h1>
      {children}
    </>
  );
}
```

### Custom Hooks
```javascript
function useCustomData() {
  const [data, setData] = useState(null);
  
  useEffect(() => {
    // Fetch or compute data
    setData({ fetched: true });
  }, []);
  
  return data;
}
```

### Conditional Rendering
```javascript
function App() {
  const [isOpen, setIsOpen] = useState(false);
  
  return (
    <>
      {isOpen && <div>Content</div>}
      <button onclick={() => setIsOpen(!isOpen)}>Toggle</button>
    </>
  );
}
```

## Limitations

While React runs fully within Artisan, there are some limitations:

1. **No actual browser APIs** - Networking through the ART `net` module, not fetch
2. **CSS is limited** - Only basic properties supported (see js_engine.h)
3. **No external npm packages** - Only what's vendored or built into the runtime
4. **Single-threaded** - No Web Workers
5. **Event system differences** - Uses real DOM events, not synthetic React events

## Performance Considerations

- React runs in the QuickJS JavaScript engine, which is fast for a JS VM but not a browser engine
- Rendering directly to the DOM tree (no virtual DOM overhead)
- Use React's optimization tools (useMemo, useCallback) for expensive computations

## Debugging

React DevTools work differently:
- Use `console.log()` for debugging (output shows in console)
- Add breakpoints using your IDE's debugger (if supported)
- React in development mode (as provided) includes extra checks and warnings

## Examples

### Example 1: Simple Counter
```jsx
function App() {
  const [count, setCount] = useState(0);
  
  return (
    <div>
      <p>Count: {count}</p>
      <button onclick={() => setCount(count + 1)}>+</button>
      <button onclick={() => setCount(count - 1)}>-</button>
    </div>
  );
}

const root = document.getElementById("root");
ReactDOM.createRoot(root).render(<App />);
```

### Example 2: Todo List
```jsx
function TodoApp() {
  const [todos, setTodos] = useState([]);
  const [input, setInput] = useState("");
  
  const addTodo = () => {
    setTodos([...todos, { id: Date.now(), text: input }]);
    setInput("");
  };
  
  return (
    <div>
      <input value={input} onchange={(e) => setInput(e.target.value)} />
      <button onclick={addTodo}>Add</button>
      <ul>
        {todos.map(todo => (
          <li key={todo.id}>{todo.text}</li>
        ))}
      </ul>
    </div>
  );
}
```

## Further Resources

- **React Documentation**: https://react.dev
- **QuickJS Engine** (js_engine.h) - Complete DOM API reference
- **ART React Module** (art/stdlib/react.ts) - Component management utilities

## Troubleshooting

### React not defined
Make sure js-prelude.js is being loaded and contains the React bundles.

### Events not firing
Check that event names are lowercase (onclick, not onClick).

### Styles not applying
Verify CSS property names are in camelCase and supported (see js_engine.h for the list).

### Components not rendering
Ensure ReactDOM.createRoot() is called with a valid DOM node and render() is invoked.
