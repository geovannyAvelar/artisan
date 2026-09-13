# React Counter Example for Artisan

A complete example of a React application running in Artisan with the built-in React 18.3.1 module. **No bundling or complex setup required!**

## Quick Start

The simplified setup with `art/react`:

```jsx
import { setupReact, createRoot, useState } from "art/react";

setupReact();  // One line to initialize!

function Counter() {
  const [count, setCount] = useState(0);
  return <button onClick={() => setCount(count + 1)}>Count: {count}</button>;
}

createRoot("root").render(<Counter />);
```

That's all you need!

## What This Example Shows

- **React 18.3.1 Built-in**: Pre-vendored, zero setup
- **Simplified Initialization**: One `setupReact()` call
- **All Hooks Supported**: useState, useEffect, useCallback, useMemo, useRef, useContext, useReducer
- **Real DOM Integration**: React renders to actual DOM nodes
- **Full Interactivity**: Event handlers, state management, conditional rendering
- **Component Composition**: Props, composition, hooks

## Project Structure

```
react-counter/
├── index.html          # HTML template with <div id="root">
├── app.jsx            # React application code (uses art/react)
└── README.md          # This file
```

## Setup (Simplified)

### Import from art/react

```javascript
import { setupReact, createRoot, useState, useEffect } from "art/react";
```

Includes:
- `setupReact()` - Initialize React
- `createRoot(selector)` - Create a React root
- `useState`, `useEffect`, `useCallback`, `useMemo`, `useRef`, `useContext`, `useReducer` - All hooks
- Helper functions: `useForm`, `useFetch`, `useAsync`, `useAnimationFrame`, `useLogger`
- Element shortcuts: `div`, `button`, `input`, `span`, `p`, `h1`, `h2`, etc.

### Example: Basic Setup

```jsx
import { setupReact, createRoot, useState } from "art/react";

setupReact();

function App() {
  const [name, setName] = useState("World");
  return <h1>Hello, {name}!</h1>;
}

createRoot("root").render(<App />);
```

## Example Components

### Counter Component

```jsx
function Counter({ initialValue = 0, step = 1 }) {
  const [count, setCount] = useState(initialValue);
  const [history, setHistory] = useState([]);
  
  const increment = () => {
    const newCount = count + step;
    setCount(newCount);
    setHistory([...history, newCount]);
  };
  
  return (
    <div style={{ padding: "20px" }}>
      <h2>Counter: {count}</h2>
      <button onClick={increment}>Increment</button>
      <p>Changes: {history.length}</p>
    </div>
  );
}
```

### Main App

```jsx
import { setupReact, createRoot, useState } from "art/react";

setupReact();

function App() {
  const [showStats, setShowStats] = useState(false);
  
  return (
    <div style={{ fontFamily: "system-ui" }}>
      <h1>React in Artisan</h1>
      <Counter initialValue={0} step={1} />
      <button onClick={() => setShowStats(!showStats)}>
        {showStats ? "Hide" : "Show"} Stats
      </button>
      {showStats && <div>Stats: ...</div>}
    </div>
  );
}

createRoot("root").render(<App />);
```

## Running This Example

### Build & Run
```bash
cd examples/react-counter
artisan build . --run
```

The app will:
1. Compile JSX to JavaScript
2. Load React 18.3.1 (pre-vendored)
3. Display the interactive counter
4. Respond to button clicks in real-time

## Available Hooks & Utilities

### Standard React Hooks
```javascript
import { 
  useState, useEffect, useCallback, useMemo, 
  useRef, useContext, useReducer 
} from "art/react";
```

### Convenience Hooks
```javascript
// Form state management
const { values, handleChange, reset } = useForm({ name: "", email: "" });

// Fetch data
const { data, loading, error } = useFetch("/api/data");

// Async operations
const { result, loading, error } = useAsync(async () => await getData());

// Animation loop
useAnimationFrame((timestamp) => {
  // Runs on each frame
});

// Debugging
useLogger("myVar", value);  // Logs value on change
usePerformance("MyComponent");  // Measures render time
```

### Element Shortcuts
```javascript
import { div, button, input, span, p, h1, h2, form, ul, li, a, img } from "art/react";

// Use directly in JSX
<div id="main">
  <h1>Title</h1>
  <button onClick={handleClick}>Click me</button>
  <input type="text" onChange={handleChange} />
</div>
```

## Common Patterns

### State-Driven UI
```jsx
function Menu() {
  const [isOpen, setIsOpen] = useState(false);
  
  return (
    <>
      <button onClick={() => setIsOpen(!isOpen)}>Menu</button>
      {isOpen && <div>Menu items...</div>}
    </>
  );
}
```

### Form Handling
```jsx
function LoginForm() {
  const { values, handleChange, reset } = useForm({
    username: "",
    password: ""
  });
  
  return (
    <form>
      <input 
        name="username" 
        value={values.username}
        onChange={handleChange}
      />
      <button onClick={() => reset()}>Clear</button>
    </form>
  );
}
```

### Data Fetching
```jsx
function DataDisplay() {
  const { data, loading, error } = useFetch("/api/items");
  
  if (loading) return <div>Loading...</div>;
  if (error) return <div>Error: {error.message}</div>;
  return <div>{data.length} items loaded</div>;
}
```

### Custom Hooks
```jsx
function useCounter(initial = 0) {
  const [count, setCount] = useState(initial);
  return {
    count,
    increment: () => setCount(c => c + 1),
    decrement: () => setCount(c => c - 1),
    reset: () => setCount(initial)
  };
}

// Usage
function App() {
  const counter = useCounter(0);
  return (
    <div>
      Count: {counter.count}
      <button onClick={counter.increment}>+</button>
    </div>
  );
}
```

## Next Steps

1. **Experiment** - Modify the counter, add features
2. **Build components** - Create reusable components
3. **Add more pages** - Use HTML routing
4. **Integrate ART** - Call ART functions from React
5. **Use modules** - Import from `art/fs`, `art/net`, etc.

## Resources

- [art/react API](../../art/stdlib/react.ts) - Full module reference
- [React Docs](https://react.dev) - Official React documentation
- [Getting Started Guide](../../docs/GETTING_STARTED.md)
- [JavaScript Guide](../../docs/JAVASCRIPT_GUIDE.md)

---

Happy React coding in Artisan! 🚀
