# React Counter Example for Artisan

This is a complete example of a React application running within Artisan using the embedded QuickJS JavaScript engine.

## What This Shows

- **React 18.3.1 Running**: Full React library (hooks, components, state management)
- **Hooks in Action**: `useState`, `useEffect`, custom logic
- **Real DOM Binding**: React rendering to actual DOM nodes through QuickJS
- **Event Handling**: Button clicks and event handlers
- **Styling**: Inline styles and CSS classes
- **Conditional Rendering**: Showing/hiding UI based on state

## Project Structure

```
react-counter/
├── index.html          # HTML template with root div
├── app.jsx            # React application code
├── js-prelude.js      # React runtime setup (loads React, sets up JSX targets)
└── README.md          # This file
```

## How It Works

### 1. Build Phase

The Artisan build system:
1. Compiles `app.jsx` to JavaScript using the JSX transformer
2. Transforms JSX expressions like `<Counter />` into `h(Counter, ...)`
3. The result is standard JavaScript

### 2. Runtime Phase

When the application runs:
1. `js-prelude.js` loads first (concatenated with React bundles)
2. It sets `h` and `Fragment` globals to `React.createElement` and `React.Fragment`
3. The compiled `app.jsx` code runs
4. React renders components to the DOM
5. User interactions trigger React state updates
6. React efficiently updates the DOM

### 3. DOM Integration

The QuickJS engine provides access to the DOM:

```javascript
// Available in your React code
document.getElementById("root")           // Get elements
element.addEventListener("click", ...)    // Attach listeners
element.style.color = "red"               // Set styles
element.appendChild(child)                // Manipulate DOM
```

## Example App Breakdown

### Counter Component

```jsx
function Counter({ initialValue = 0, step = 1 }) {
  const [count, setCount] = useState(initialValue);
  const [history, setHistory] = useState([]);
  
  // ... render UI with buttons that update state
}
```

Features:
- **Props**: Takes `initialValue` and `step` parameters
- **State**: Tracks count and history of changes
- **Hooks**: Uses `useState` for state management
- **Rendering**: Displays count, action buttons, and statistics

### Main App Component

```jsx
function App() {
  const [showStats, setShowStats] = useState(false);
  
  return (
    // Renders Counter component with default props
    // Toggle-able statistics display
  );
}
```

Features:
- **Composition**: Renders the Counter component
- **Conditional UI**: Shows/hides statistics based on state
- **Interactivity**: Buttons trigger state changes

## Running This Example

### Prerequisites
- Artisan framework installed
- Node.js (for build tooling)

### Build
```bash
cd examples/react-counter
artisan build
```

### Run
```bash
artisan run
```

The app will:
1. Load in your default browser
2. Display the counter interface
3. Be fully interactive - click buttons to increment/decrement
4. Show history and statistics when enabled

## Key React Concepts Used

### State Management
```javascript
const [count, setCount] = useState(0);
setCount(count + 1);  // Update state
```

### Effects
```javascript
useEffect(() => {
  console.log("Component mounted");
  return () => console.log("Component unmounted");
}, []);
```

### Props & Destructuring
```javascript
function Counter({ initialValue = 0, step = 1 }) {
  // Use props with default values
}
```

### Event Handlers
```javascript
<button onclick={() => setCount(count + 1)}>
  Increment
</button>
```

Note: Event names are lowercase (onclick, not onClick) to match real DOM conventions.

### Conditional Rendering
```javascript
{showStats && <div>Statistics...</div>}
```

### Lists & Array Methods
```javascript
{history.map(value => <span>{value}</span>)}
```

## Styling in React/Artisan

### Inline Styles (camelCase)
```jsx
<div style={{ color: "red", backgroundColor: "blue" }}>
  Styled content
</div>
```

### CSS Classes
```jsx
<div class="my-class">Content</div>
```

### Direct DOM Manipulation
```javascript
element.style.color = "red";
element.classList.add("active");
```

## Console Output

When running, you'll see console output:
- "React runtime initialized" - js-prelude.js has loaded
- "Counter initialized with value: 0" - useEffect ran
- Any console.log() statements from your code

## Common Patterns You Can Use

### Custom Hooks
```javascript
function useCounter(initial = 0) {
  const [count, setCount] = useState(initial);
  return [count, (delta) => setCount(count + delta)];
}
```

### Component Composition
```javascript
function Parent() {
  return <Child prop1="value" />;
}

function Child({ prop1 }) {
  return <div>{prop1}</div>;
}
```

### State-Driven UI
```javascript
const [isOpen, setIsOpen] = useState(false);
return (
  <>
    {isOpen && <Menu />}
    <button onclick={() => setIsOpen(!isOpen)}>Toggle Menu</button>
  </>
);
```

## Limitations & Notes

1. **No fetch/HTTP** - Use Artisan's `net` module for networking
2. **Limited CSS** - Only basic properties are supported (color, background-color, etc.)
3. **No npm packages** - Only React and what's built into Artisan
4. **Single event loop** - No true async/await; use Promises from `art/promise`
5. **Event naming** - Uses real DOM names (onclick, not onClick)

## Next Steps

1. **Modify this example** - Try changing the counter logic
2. **Create new components** - Add your own React components
3. **Add more interactivity** - Use more Hooks (useCallback, useEffect, etc.)
4. **Integrate with ART** - Call ART functions from React or vice versa
5. **Use Artisan modules** - Import from `art/fs`, `art/net`, etc.

## Resources

- [React Documentation](https://react.dev)
- [Artisan README](../../README.md)
- [React Integration Guide](../../REACT_INTEGRATION.md)
- [Artisan Stdlib Modules](../../art/stdlib/)

## Troubleshooting

### "React is not defined"
Make sure js-prelude.js is loaded with the React bundles.

### Buttons not responding
Check browser console for errors. Event handlers use `onclick` (lowercase), not `onClick`.

### Styling not working
Verify CSS property names are camelCase and supported. Not all CSS properties are available.

### Component not rendering
Ensure ReactDOM.createRoot() is called and render() is invoked on the root.

---

Happy React coding in Artisan! 🚀
