# Bundling React Packages for Artisan

This example demonstrates how to bundle React packages (React Router, state management, etc.) for use within Artisan.

## What This Example Shows

- Using pre-vendored React (already included in Artisan)
- Bundling additional React packages (React Router, etc.)
- Registering bundled React packages as modules
- Combining React with Artisan's module system

## Quick Start

### 1. React is Already Available

Artisan includes React 18.3.1 out of the box:
```typescript
import { require } from "art/modules";

const React = require("react");
const ReactDOM = require("react-dom");
```

### 2. Bundle Additional Packages

To bundle React Router or other packages:

```bash
# Install packages
npm install react-router react-router-dom

# Bundle
esbuild bundle-react-router.js --bundle --format=iife --outfile=react-router.js
```

### 3. Register and Use

```typescript
import { registerModule, require } from "art/modules";

// Register bundled React Router
registerModule("react-router", function(module, exports, require) {
  const React = require("react");
  // Include bundled code here
  exports.BrowserRouter = React.Component({...});
});

// Use it
const Router = require("react-router");
```

## Directory Structure

```
examples/bundling/react/
  ├── README.md                    # This file
  ├── app.jsx                      # React application (uses already-vendored React)
  ├── app.ts                       # ART integration code
  ├── package.json                 # Dependencies
  ├── bundle-react-router.js       # React Router entry point (optional)
  └── bundles/                     # Output bundles (optional)
      └── react-router.js
```

## React in Artisan

React is already integrated through the examples/react-counter project. Here's a quick overview:

### Pre-registered React

```typescript
// React is pre-registered by the JS runtime
import { require } from "art/modules";

const React = require("react");
const ReactDOM = require("react-dom");

// Create a component
function App() {
  return <h1>Hello React!</h1>;
}

// Render to DOM
ReactDOM.render(<App />, document.getElementById("root"));
```

### Using React Hooks

```typescript
import React, { useState, useEffect } from "react";

function Counter() {
  const [count, setCount] = useState(0);
  
  useEffect(() => {
    console.log("Component mounted!");
    return () => console.log("Component will unmount");
  }, []);
  
  return (
    <div>
      <p>Count: {count}</p>
      <button onClick={() => setCount(count + 1)}>Increment</button>
    </div>
  );
}
```

### Component Organization

Structure your React app with the module system:

```typescript
// modules/Button.jsx
export function Button({ onClick, children }) {
  return <button onClick={onClick}>{children}</button>;
}

// modules/Counter.jsx
import { Button } from "./Button";

export function Counter() {
  const [count, setCount] = useState(0);
  return (
    <div>
      <p>Count: {count}</p>
      <Button onClick={() => setCount(count + 1)}>+</Button>
    </div>
  );
}

// app.ts
import { require } from "art/modules";
import Counter from "./modules/Counter";

const React = require("react");
const root = React.createRoot(document.getElementById("root"));
root.render(<Counter />);
```

## Bundling Additional React Packages

### React Router Example

Create entry point:
```javascript
// bundle-react-router.js
export * from "react-router";
export * from "react-router-dom";
```

Bundle:
```bash
esbuild bundle-react-router.js \
  --bundle \
  --format=iife \
  --external:react \
  --external:react-dom \
  --outfile=react-router.js
```

Use:
```typescript
const Router = require("react-router");
const { BrowserRouter, Routes, Route } = Router;

function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<Home />} />
        <Route path="/about" element={<About />} />
      </Routes>
    </BrowserRouter>
  );
}
```

### State Management (Redux)

Bundle Redux:
```bash
npm install redux react-redux

esbuild bundle-redux.js --bundle \
  --format=iife \
  --external:react \
  --outfile=redux.js
```

Use:
```typescript
const Redux = require("redux");
const ReactRedux = require("react-redux");

const reducer = (state = 0, action: any) => {
  if (action.type === "INCREMENT") return state + 1;
  return state;
};

const store = Redux.createStore(reducer);

function App() {
  return (
    <ReactRedux.Provider store={store}>
      <Counter />
    </ReactRedux.Provider>
  );
}
```

## Comparison: React vs Angular in Artisan

| Feature | React | Angular |
|---------|-------|---------|
| Bundle size | ~40KB | ~500KB+ |
| Learning curve | Easy | Steep |
| Pre-bundled | ✅ Yes | ❌ Requires bundling |
| JSX support | ✅ Native | ❌ Requires setup |
| Community | Huge | Large |
| UI state | Hooks (simple) | RxJS (complex) |
| Recommended for | UI apps | Enterprise apps |

## Performance Tips

### 1. Use Code Splitting
```typescript
const Component = React.lazy(() => import("./Component"));

function App() {
  return (
    <React.Suspense fallback={<div>Loading...</div>}>
      <Component />
    </React.Suspense>
  );
}
```

### 2. Memoize Components
```typescript
const MemoComponent = React.memo(({ data }) => {
  return <div>{data}</div>;
});
```

### 3. Use useCallback
```typescript
const MyComponent = ({ onUpdate }) => {
  const handleClick = React.useCallback(() => {
    onUpdate();
  }, [onUpdate]);
  
  return <button onClick={handleClick}>Update</button>;
};
```

## Bundling Strategy for React

### Option 1: Use Pre-vendored React (Recommended)
- React 18.3.1 already available
- No bundling needed
- Fast startup
- Perfect for most apps

### Option 2: Bundle React Ecosystem
- Bundle additional packages separately
- Keep React core pre-vendored
- Load extras on demand
- Good for complex apps

### Option 3: Custom React Build
- Bundle custom React configuration
- For advanced use cases
- Requires more setup

## Common React Patterns in Artisan

### Global State with useContext
```typescript
const ThemeContext = React.createContext("light");

function App() {
  const [theme, setTheme] = React.useState("light");
  
  return (
    <ThemeContext.Provider value={theme}>
      <ThemedComponent />
    </ThemeContext.Provider>
  );
}

function ThemedComponent() {
  const theme = React.useContext(ThemeContext);
  return <div className={theme}>Themed</div>;
}
```

### Form Handling
```typescript
function MyForm() {
  const [formData, setFormData] = React.useState({
    name: "",
    email: ""
  });
  
  const handleChange = (e: any) => {
    setFormData({
      ...formData,
      [e.target.name]: e.target.value
    });
  };
  
  return (
    <form>
      <input name="name" onChange={handleChange} value={formData.name} />
      <input name="email" onChange={handleChange} value={formData.email} />
    </form>
  );
}
```

### Async Data Fetching
```typescript
function DataComponent() {
  const [data, setData] = React.useState(null);
  
  React.useEffect(() => {
    fetch("/api/data")
      .then(r => r.json())
      .then(setData);
  }, []);
  
  return <div>{data ? JSON.stringify(data) : "Loading..."}</div>;
}
```

## Troubleshooting

### "React is not defined"
**Problem**: React not registered before component rendering
**Solution**: Ensure React is required before creating components

### "Cannot find module 'react'"
**Problem**: React module not available
**Solution**: React should be pre-registered. Check if module system is initialized.

### Component not re-rendering
**Problem**: State changes not triggering re-renders
**Solution**: Use proper state setter (not mutations):
```typescript
// ❌ Wrong
state.items.push(newItem);

// ✅ Right
setItems([...items, newItem]);
```

## Next Steps

1. Start with the pre-vendored React for UI development
2. If you need additional packages, bundle them separately
3. Organize components using the module system
4. For large apps, consider code splitting with React.lazy

## References

- React Documentation: https://react.dev/
- React Hooks Guide: https://react.dev/reference/react/hooks
- Artisan React Integration: See examples/react-counter
- Module System: See MODULES.md
- Bundling Guide: See BUNDLING.md
