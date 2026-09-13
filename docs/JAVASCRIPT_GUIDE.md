# JavaScript & React Guide

Using JavaScript and React 18.3.1 in Artisan applications.

## Overview

Artisan includes **React 18.3.1 pre-vendored** and runs JavaScript via the **QuickJS** engine. No bundling, no additional setup needed.

**Key points:**
- React is ready to use immediately
- Full hooks support (useState, useEffect, useCallback, etc.)
- Both CommonJS and ES6 modules supported
- Seamless integration with ART code
- Native performance for JavaScript execution

## Quick Start (Simplified)

**Use the built-in `art/react` module for zero setup!**

Create `app.jsx`:

```javascript
import { setupReact, createRoot, useState } from "art/react";

setupReact();  // One line to initialize!

function Counter() {
  const [count, setCount] = useState(0);
  
  return (
    <div>
      <p>Count: {count}</p>
      <button onClick={() => setCount(count + 1)}>
        Increment
      </button>
    </div>
  );
}

createRoot("root").render(<Counter />);
```

Create `pages/index.html`:

```html
<div id="root"></div>
```

Build and run:

```bash
artisan-cli build my-app --run
```

Done! React is running with zero configuration.

Create `pages/index.html`:

```html
<div id="root"></div>
```

Build and run:

```bash
artisan-cli build my-app --run
```

Done! React is running.

## Why React?

**Advantages:**
- Component-based UI development
- Hooks for state management
- Large ecosystem and community
- Familiar to web developers
- Hot-reload friendly during development

**When to use:**
- Complex, interactive UIs
- Rapid prototyping
- Component reuse
- State management at scale

**When to use ART instead:**
- Performance-critical code
- Simple, mostly-static UIs
- Small binary size is critical

## React Basics

### Functional Components

```javascript
function Welcome() {
  return <h1>Hello, World!</h1>;
}

function Welcome(props) {
  return <h1>Hello, {props.name}!</h1>;
}
```

### Props

```javascript
function Greeting({ name, age }) {
  return (
    <div>
      <p>Name: {name}</p>
      <p>Age: {age}</p>
    </div>
  );
}

// Use with JSX
<Greeting name="Alice" age={30} />
```

### useState Hook

```javascript
import React, { useState } from "react";

function Counter() {
  const [count, setCount] = useState(0);
  
  return (
    <div>
      <p>Count: {count}</p>
      <button onClick={() => setCount(count + 1)}>+</button>
      <button onClick={() => setCount(count - 1)}>-</button>
      <button onClick={() => setCount(0)}>Reset</button>
    </div>
  );
}
```

### useEffect Hook

```javascript
import React, { useEffect, useState } from "react";

function DataFetcher() {
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);
  
  useEffect(() => {
    // Runs once on mount
    fetch("/api/data")
      .then(r => r.json())
      .then(d => {
        setData(d);
        setLoading(false);
      });
  }, []);  // Empty deps = run once
  
  if (loading) return <div>Loading...</div>;
  return <div>{JSON.stringify(data)}</div>;
}
```

### useCallback Hook

```javascript
import React, { useCallback } from "react";

function Parent() {
  const [count, setCount] = React.useState(0);
  
  // Memoize callback - only recreates if dependencies change
  const handleClick = useCallback(() => {
    setCount(c => c + 1);
  }, []);
  
  return <Child onClick={handleClick} />;
}

function Child({ onClick }) {
  return <button onClick={onClick}>Click</button>;
}
```

### useMemo Hook

```javascript
import React, { useMemo } from "react";

function ExpensiveComponent({ items }) {
  // Only recompute when items changes
  const filtered = useMemo(() => {
    console.log("Computing...");
    return items.filter(x => x > 10);
  }, [items]);
  
  return <div>{filtered.join(", ")}</div>;
}
```

### useRef Hook

```javascript
import React, { useRef } from "react";

function TextInput() {
  const inputRef = useRef(null);
  
  const focusInput = () => {
    if (inputRef.current) {
      inputRef.current.focus();
    }
  };
  
  return (
    <div>
      <input ref={inputRef} />
      <button onClick={focusInput}>Focus input</button>
    </div>
  );
}
```

## State Management

### Local State (useState)

For component-level state:

```javascript
function TodoItem({ todo }) {
  const [editing, setEditing] = React.useState(false);
  
  return (
    <div>
      {editing ? (
        <input defaultValue={todo.text} />
      ) : (
        <span>{todo.text}</span>
      )}
      <button onClick={() => setEditing(!editing)}>
        {editing ? "Save" : "Edit"}
      </button>
    </div>
  );
}
```

### Context API (Global State)

For app-level state shared across components:

```javascript
import React, { createContext, useContext, useState } from "react";

// Create context
const ThemeContext = createContext("light");

// Provider component
function ThemeProvider({ children }) {
  const [theme, setTheme] = useState("light");
  
  return (
    <ThemeContext.Provider value={{ theme, setTheme }}>
      {children}
    </ThemeContext.Provider>
  );
}

// Use in components
function ThemedButton() {
  const { theme } = useContext(ThemeContext);
  return <button className={theme}>Click me</button>;
}

// App setup
function App() {
  return (
    <ThemeProvider>
      <ThemedButton />
    </ThemeProvider>
  );
}
```

### Custom Hooks

```javascript
function useWindowSize() {
  const [size, setSize] = React.useState({
    width: window.innerWidth,
    height: window.innerHeight
  });
  
  React.useEffect(() => {
    const handleResize = () => {
      setSize({
        width: window.innerWidth,
        height: window.innerHeight
      });
    };
    
    window.addEventListener("resize", handleResize);
    return () => window.removeEventListener("resize", handleResize);
  }, []);
  
  return size;
}

// Use it
function App() {
  const size = useWindowSize();
  return <div>Width: {size.width}, Height: {size.height}</div>;
}
```

## Common Patterns

### Form Handling

```javascript
function LoginForm() {
  const [formData, setFormData] = React.useState({
    username: "",
    password: ""
  });
  
  const handleChange = (e) => {
    const { name, value } = e.target;
    setFormData(prev => ({
      ...prev,
      [name]: value
    }));
  };
  
  const handleSubmit = (e) => {
    e.preventDefault();
    console.log("Submitting:", formData);
  };
  
  return (
    <form onSubmit={handleSubmit}>
      <input
        name="username"
        value={formData.username}
        onChange={handleChange}
        placeholder="Username"
      />
      <input
        name="password"
        type="password"
        value={formData.password}
        onChange={handleChange}
        placeholder="Password"
      />
      <button type="submit">Login</button>
    </form>
  );
}
```

### Conditional Rendering

```javascript
function Message({ status }) {
  if (status === "loading") {
    return <div>Loading...</div>;
  }
  
  if (status === "error") {
    return <div>Error occurred</div>;
  }
  
  return <div>Success!</div>;
}

// Or with ternary
function Status({ isOnline }) {
  return <span>{isOnline ? "Online" : "Offline"}</span>;
}

// Or with &&
function Notification({ message }) {
  return message && <div>{message}</div>;
}
```

### List Rendering

```javascript
function TodoList({ todos }) {
  return (
    <ul>
      {todos.map(todo => (
        <li key={todo.id}>
          {todo.text}
          <button>Delete</button>
        </li>
      ))}
    </ul>
  );
}
```

## Module System

Organize your React code with modules:

```javascript
// modules/Button.jsx
export function Button({ label, onClick }) {
  return <button onClick={onClick}>{label}</button>;
}

// modules/Counter.jsx
import React, { useState } from "react";
import { Button } from "./Button";

export function Counter() {
  const [count, setCount] = useState(0);
  
  return (
    <div>
      <p>Count: {count}</p>
      <Button label="+" onClick={() => setCount(count + 1)} />
      <Button label="-" onClick={() => setCount(count - 1)} />
    </div>
  );
}

// app.jsx
import React from "react";
import ReactDOM from "react-dom/client";
import { Counter } from "./modules/Counter";

ReactDOM.createRoot(document.getElementById("root")).render(<Counter />);
```

See [Module System Guide](MODULES.md) for more details.

## Using Angular Instead of React

Artisan also has **built-in Angular support** with:
- @angular/core, @angular/common, @angular/forms
- RxJS for reactive programming
- Full dependency injection and decorators

```typescript
import { setupAngular, createFormGroup } from "art/angular";

setupAngular();

const Angular = require("@angular/core");

@Angular.Component({
  selector: "app-counter",
  template: "<div>Angular component</div>"
})
class CounterComponent {
  count: number = 0;
}

// Forms work the same way
const form = createFormGroup({
  username: [""],
  email: [""]
});
```

Use Angular when:
- You prefer Angular's patterns and structure
- You want TypeScript decorators
- You need reactive forms
- You're familiar with Angular ecosystem

See [Angular integration guide](../examples/bundling/angular/README.md) for complete examples.

## Combining React with ART

Use React for UI, ART for performance-critical code:

```typescript
// app.tsx (ART)
import { Node } from "art";

export function setupArtFeatures(): void {
  let container: Node = document.getElementById("art-features");
  // Build native UI with ART
}
```

```javascript
// app.jsx (React)
import React from "react";
import ReactDOM from "react-dom/client";
import { setupArtFeatures } from "./app";

function App() {
  React.useEffect(() => {
    setupArtFeatures();
  }, []);
  
  return (
    <div>
      <div id="react-root">React UI</div>
      <div id="art-features">ART UI</div>
    </div>
  );
}

ReactDOM.createRoot(document.getElementById("root")).render(<App />);
```

## DOM Access

Both ART and React share the same DOM:

```javascript
// React can access elements created by ART
const artButton = document.getElementById("art-button");

// ART can access elements created by React
const reactDiv = document.querySelector(".react-component");
```

Be careful with:
- Multiple frameworks manipulating same elements
- React virtual DOM vs ART direct manipulation
- State synchronization between them

## Styling

### Inline Styles

```javascript
function StyledComponent() {
  const styles = {
    container: {
      padding: "20px",
      backgroundColor: "#f0f0f0"
    },
    title: {
      fontSize: "24px",
      color: "blue"
    }
  };
  
  return (
    <div style={styles.container}>
      <h1 style={styles.title}>Title</h1>
    </div>
  );
}
```

### CSS Classes

```javascript
function Button({ disabled }) {
  const className = disabled ? "btn btn-disabled" : "btn btn-primary";
  return <button className={className}>Click</button>;
}
```

### Conditional Styles

```javascript
function StatusBox({ status }) {
  const style = {
    backgroundColor: status === "error" ? "red" : "green",
    color: "white",
    padding: "10px"
  };
  
  return <div style={style}>{status}</div>;
}
```

## Performance Tips

1. **Use React.memo for expensive components**
   ```javascript
   const Button = React.memo(({ label, onClick }) => (
     <button onClick={onClick}>{label}</button>
   ));
   ```

2. **Use useCallback to memoize callbacks**
   ```javascript
   const handleClick = useCallback(() => {
     setCount(c => c + 1);
   }, []);
   ```

3. **Use useMemo for expensive computations**
   ```javascript
   const filtered = useMemo(() => items.filter(x => x > 10), [items]);
   ```

4. **Avoid creating objects/arrays in render**
   ```javascript
   // ✗ Creates new array every render
   <Child items={[1, 2, 3]} />
   
   // ✓ Reuse const
   const items = [1, 2, 3];
   <Child items={items} />
   ```

5. **Use key properly in lists**
   ```javascript
   // ✓ Correct - stable unique key
   <ul>
     {items.map(item => <li key={item.id}>{item.name}</li>)}
   </ul>
   
   // ✗ Wrong - index as key can cause issues
   <ul>
     {items.map((item, i) => <li key={i}>{item.name}</li>)}
   </ul>
   ```

## Debugging

### React DevTools

Use browser React DevTools extension if available.

### Console Logging

```javascript
function Component() {
  React.useEffect(() => {
    console.log("Component mounted");
    return () => console.log("Component unmounted");
  }, []);
  
  return <div>Content</div>;
}
```

### React.StrictMode

Wrap app to get extra checks:

```javascript
import React from "react";
import ReactDOM from "react-dom/client";
import App from "./App";

ReactDOM.createRoot(document.getElementById("root")).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);
```

## Common Pitfalls

### Missing Dependencies in useEffect

```javascript
// ✗ Bug - missing dependency
useEffect(() => {
  const timer = setInterval(() => {
    setCount(count + 1);  // count is stale!
  }, 1000);
}, []);

// ✓ Correct
useEffect(() => {
  const timer = setInterval(() => {
    setCount(c => c + 1);  // Use updater function
  }, 1000);
}, []);
```

### Direct State Mutation

```javascript
// ✗ Bug - mutating state directly
const user = { name: "Alice" };
user.name = "Bob";
setUser(user);

// ✓ Correct
setUser({ ...user, name: "Bob" });
```

### Rendering in loops

```javascript
// ✗ Bug
for (let i = 0; i < 5; i++) {
  root.render(<Component i={i} />);
}

// ✓ Correct
root.render(
  <div>
    {[0, 1, 2, 3, 4].map(i => <Component key={i} i={i} />)}
  </div>
);
```

## Resources

- [React Docs](https://react.dev/)
- [React Hooks Reference](https://react.dev/reference/react)
- [examples/react-counter](../examples/react-counter/) - Complete example

---

See also:
- [ART Language Guide](ART_GUIDE.md) - Compiled TypeScript alternative
- [Module System](MODULES.md) - Organizing code
- [Getting Started](GETTING_STARTED.md) - Project setup
