// js-prelude.js - React Setup for Artisan
// This file is embedded before app.jsx and sets up the React runtime.
//
// In a real Artisan project, this would be concatenated with:
// - third_party/react/react.development.js
// - third_party/react/react-dom.development.js
// 
// Then this setup code runs to redirect JSX compilation targets.

// Assume React and ReactDOM are loaded globally by the embedder
// (In a real scenario, the file would be concatenated as follows:
//   cat react.development.js react-dom.development.js js-prelude.js > react-runtime.js
// )

// This section runs AFTER React and ReactDOM are loaded.

// Make React available globally
if (typeof globalThis !== 'undefined') {
  // Ensure React and ReactDOM are available
  if (typeof window !== 'undefined' && window.React) {
    globalThis.React = window.React;
    globalThis.ReactDOM = window.ReactDOM;
  }
}

// Redirect JSX compilation targets
// Every .jsx file compiles to calls to h() and Fragment()
// By setting these to React's implementations, JSX works with real React

globalThis.h = function(tag, props, ...children) {
  if (typeof React !== 'undefined' && React.createElement) {
    // Use React's createElement
    return React.createElement(tag, props, ...children);
  }
  // Fallback - this shouldn't happen if React is properly loaded
  return { type: tag, props: props, children: children };
};

globalThis.Fragment = function(props) {
  if (typeof React !== 'undefined' && React.Fragment) {
    // Use React's Fragment
    const React_Fragment = React.Fragment;
    return React.createElement(React_Fragment, props, props.children);
  }
  // Fallback
  return { type: 'fragment', children: props.children };
};

// Helper to mount React apps
globalThis.mountApp = function(App, mountPoint) {
  if (typeof ReactDOM !== 'undefined' && ReactDOM.createRoot) {
    const root = ReactDOM.createRoot(mountPoint);
    root.render(React.createElement(App));
    return root;
  }
  console.error("React or ReactDOM not available");
  return null;
};

console.log("React runtime initialized. Ready for JSX compilation.");
