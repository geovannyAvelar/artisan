// React integration module for ART. Import with: `import { setupReact, createReactRoot, ... } from "art/react";`
// Provides utilities for running React applications through QuickJS.

// React context type for managing the React runtime
export type ReactContext = number;

// React component type
export type ReactComponent = (props: any) => any;

// React element type
export type ReactElement = any;

// Root rendering context
type RootContext = [rootNode: any, isDirty: boolean, suspense: boolean];
let _reactContexts: RootContext[] = [];
let _contextCounter: number = 0;

// Initialize React in the QuickJS context. Must be called before using React.
// Loads react.development.js and react-dom.development.js
export function setupReact(): boolean {
  // This would be called from C++ or through a mechanism that has access to
  // the vendored React files in third_party/react/
  // In practice, the embedding application (like an Artisan CLI command)
  // would handle loading react-runtime.js (which concatenates the bundles)
  // before invoking the script engine.
  return true;
}

// Creates a React root and mounts the given component to a DOM node.
// In React 18+, this replaces the old ReactDOM.render() API.
export function createReactRoot(domNode: any, component: ReactComponent): ReactContext {
  let contextId: ReactContext = _contextCounter;
  _contextCounter = _contextCounter + 1;
  
  // Store the rendering context
  _reactContexts = _reactContexts + [[domNode, true, false]];
  
  return contextId;
}

// Renders a React component to a root created with createReactRoot().
// This is called to actually render the component after createReactRoot().
export function renderToRoot(context: ReactContext, component: ReactComponent): boolean {
  if (context < 0 || context >= _reactContexts.length) { return false; }
  
  let root: RootContext = _reactContexts[context];
  
  // Mark the root as needing redraw
  root[1] = true;
  
  // In actual implementation, this would call React's root.render(component)
  // through the QuickJS binding layer
  
  return true;
}

// Creates a simple functional component wrapper.
// Components in React are just functions that return elements.
export function createComponent(renderFn: (props: any) => ReactElement): ReactComponent {
  // Return a function that matches the React component signature
  return renderFn;
}

// Hooks API wrapper for useState - manages component state.
// Note: This requires actual React to be loaded in QuickJS context.
// The real implementation would call React.useState directly.
export function useState<T>(initialValue: T): [T, (newValue: T) => void] {
  // Placeholder - the real implementation uses React.useState from QuickJS
  let currentValue: T = initialValue;
  
  let setter: (newValue: T) => void = function(newValue: T): void {
    currentValue = newValue;
  };
  
  return [currentValue, setter];
}

// Hooks API wrapper for useEffect.
// Registers a side effect that runs when dependencies change.
export function useEffect(fn: () => void, dependencies: any[]): void {
  // Placeholder - the real implementation uses React.useEffect from QuickJS
  fn();
}

// Hooks API wrapper for useCallback.
// Memoizes a callback to prevent unnecessary re-renders.
export function useCallback<T extends (...args: any[]) => any>(
  callback: T,
  dependencies: any[]
): T {
  // Placeholder - the real implementation uses React.useCallback from QuickJS
  return callback;
}

// Hooks API wrapper for useMemo.
// Memoizes a computed value to prevent recalculation.
export function useMemo<T>(fn: () => T, dependencies: any[]): T {
  // Placeholder - the real implementation uses React.useMemo from QuickJS
  return fn();
}

// Creates a React element (equivalent to React.createElement or JSX <tag />).
// This is used internally by the JSX transform target.
export function createElement(
  tagOrComponent: string | ReactComponent,
  props: any,
  ...children: any[]
): ReactElement {
  // Placeholder - real implementation delegates to React.createElement in QuickJS
  return {
    type: tagOrComponent,
    props: props,
    children: children,
  };
}

// Fragment component (equivalent to React.Fragment or <>...</>).
// Renders multiple children without a wrapper element.
export function Fragment(props: any): ReactElement {
  return {
    type: "fragment",
    props: props,
    children: props.children,
  };
}

// Utility: Checks if a value is a valid React element.
export function isReactElement(value: any): boolean {
  if (typeof value != "object") { return false; }
  if (value == null) { return false; }
  // A simple heuristic - real React uses Symbol.for('react.element')
  return value.type != null && (value.props != null || value.children != null);
}

// Utility: Retrieves the root node from a React context.
export function getRootNode(context: ReactContext): any {
  if (context < 0 || context >= _reactContexts.length) { return null; }
  return _reactContexts[context][0];
}

// Utility: Checks if a root needs to be redrawn.
export function isRootDirty(context: ReactContext): boolean {
  if (context < 0 || context >= _reactContexts.length) { return false; }
  return _reactContexts[context][1];
}

// Utility: Marks a root as clean (redrawn).
export function markRootClean(context: ReactContext): boolean {
  if (context < 0 || context >= _reactContexts.length) { return false; }
  let root: RootContext = _reactContexts[context];
  root[1] = false;
  return true;
}

// Gets the total number of React roots created.
export function getRootCount(): number {
  return _reactContexts.length;
}

// Clears all React contexts (useful for testing).
export function clearReactContexts(): void {
  _reactContexts = [];
  _contextCounter = 0;
}

// Advanced: Suspense support for React concurrent features.
// Marks a root as using suspense for lazy loading.
export function enableSuspense(context: ReactContext): boolean {
  if (context < 0 || context >= _reactContexts.length) { return false; }
  let root: RootContext = _reactContexts[context];
  root[2] = true;
  return true;
}

// Advanced: Check if a root has suspense enabled.
export function hasSuspense(context: ReactContext): boolean {
  if (context < 0 || context >= _reactContexts.length) { return false; }
  return _reactContexts[context][2];
}
