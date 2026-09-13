// React integration module for ART. Import with: `import { setupReact, createRoot, ... } from "art/react";`
// Provides simplified React 18.3.1 setup and comprehensive utilities.

// Get React from the pre-vendored module
function getReact(): any {
  return (globalThis as any).React;
}

function getReactDOM(): any {
  return (globalThis as any).ReactDOM;
}

// ============================================================================
// Setup
// ============================================================================

// Initialize React - call this once at startup
export function setupReact(): boolean {
  // React 18.3.1 is pre-vendored and available globally
  const React = getReact();
  const ReactDOM = getReactDOM();

  if (!React || !ReactDOM) {
    console.warn("React or ReactDOM not found. Is it pre-vendored?");
    return false;
  }

  return true;
}

// Create a React root and mount a component to a DOM node
export function createRoot(container: HTMLElement | string): any {
  const ReactDOM = getReactDOM();

  if (typeof container === "string") {
    const el = document.getElementById(container);
    if (!el) {
      console.error("Container element not found:", container);
      return null;
    }
    return ReactDOM.createRoot(el);
  }

  return ReactDOM.createRoot(container);
}

// Shortcut to create and render a component in one call
export function render(component: any, container: HTMLElement | string): boolean {
  const root = createRoot(container);
  if (!root) return false;

  root.render(component);
  return true;
}

// ============================================================================
// Hooks
// ============================================================================

// useState hook
export function useState<T>(initialValue: T | (() => T)): [T, (value: T | ((prev: T) => T)) => void] {
  const React = getReact();
  return React.useState(initialValue);
}

// useEffect hook
export function useEffect(
  effect: () => void | (() => void),
  deps?: any[]
): void {
  const React = getReact();
  return React.useEffect(effect, deps);
}

// useCallback hook
export function useCallback<T extends (...args: any[]) => any>(
  callback: T,
  deps: any[]
): T {
  const React = getReact();
  return React.useCallback(callback, deps);
}

// useMemo hook
export function useMemo<T>(
  factory: () => T,
  deps: any[]
): T {
  const React = getReact();
  return React.useMemo(factory, deps);
}

// useRef hook
export function useRef<T>(initialValue: T): { current: T } {
  const React = getReact();
  return React.useRef(initialValue);
}

// useContext hook
export function useContext<T>(context: any): T {
  const React = getReact();
  return React.useContext(context);
}

// useReducer hook
export function useReducer<S, A>(
  reducer: (state: S, action: A) => S,
  initialState: S,
  init?: (initial: S) => S
): [S, (action: A) => void] {
  const React = getReact();
  if (init) {
    return React.useReducer(reducer, initialState, init);
  }
  return React.useReducer(reducer, initialState);
}

// ============================================================================
// Context and Components
// ============================================================================

// Create a context for state management
export function createContext<T>(defaultValue: T): any {
  const React = getReact();
  return React.createContext(defaultValue);
}

// Memo component for performance optimization
export function memo<P>(
  Component: (props: P) => any,
  propsAreEqual?: (prevProps: P, nextProps: P) => boolean
): any {
  const React = getReact();
  return React.memo(Component, propsAreEqual);
}

// ============================================================================
// JSX and Element Creation
// ============================================================================

// Create a React element
export function createElement<P>(
  type: string | ((props: any) => any),
  props?: P | null,
  ...children: any[]
): any {
  const React = getReact();
  return React.createElement(type, props, ...children);
}

// Commonly used element shortcuts
export function div(props?: any, ...children: any[]): any {
  return createElement("div", props, ...children);
}

export function button(props?: any, ...children: any[]): any {
  return createElement("button", props, ...children);
}

export function input(props?: any): any {
  return createElement("input", props);
}

export function span(props?: any, ...children: any[]): any {
  return createElement("span", props, ...children);
}

export function p(props?: any, ...children: any[]): any {
  return createElement("p", props, ...children);
}

export function h1(props?: any, ...children: any[]): any {
  return createElement("h1", props, ...children);
}

export function h2(props?: any, ...children: any[]): any {
  return createElement("h2", props, ...children);
}

export function h3(props?: any, ...children: any[]): any {
  return createElement("h3", props, ...children);
}

export function h4(props?: any, ...children: any[]): any {
  return createElement("h4", props, ...children);
}

export function h5(props?: any, ...children: any[]): any {
  return createElement("h5", props, ...children);
}

export function form(props?: any, ...children: any[]): any {
  return createElement("form", props, ...children);
}

export function ul(props?: any, ...children: any[]): any {
  return createElement("ul", props, ...children);
}

export function ol(props?: any, ...children: any[]): any {
  return createElement("ol", props, ...children);
}

export function li(props?: any, ...children: any[]): any {
  return createElement("li", props, ...children);
}

export function a(props?: any, ...children: any[]): any {
  return createElement("a", props, ...children);
}

export function img(props?: any): any {
  return createElement("img", props);
}

export function Fragment(props: any): any {
  const React = getReact();
  return React.Fragment;
}

// ============================================================================
// Form Utilities
// ============================================================================

// Hook for managing form state
export function useForm<T extends Record<string, any>>(
  initialValues: T
): {
  values: T;
  setValues: (values: Partial<T>) => void;
  handleChange: (e: any) => void;
  reset: () => void;
} {
  const [values, setValues] = useState(initialValues);
  const initialValuesRef = useRef(initialValues);

  const handleChange = (e: any) => {
    const { name, value, type, checked } = e.target;
    setValues({
      ...values,
      [name]: type === "checkbox" ? checked : value
    });
  };

  const reset = () => {
    setValues(initialValuesRef.current);
  };

  return {
    values,
    setValues: (v: Partial<T>) => setValues({ ...values, ...v }),
    handleChange,
    reset
  };
}

// ============================================================================
// Common Patterns
// ============================================================================

// Conditional rendering helper
export function conditional<T>(
  condition: boolean,
  trueComponent: T | null,
  falseComponent?: T | null
): T | null {
  return condition ? trueComponent : (falseComponent || null);
}

// Map over items to create components
export function mapComponents<T, R>(
  items: T[],
  renderItem: (item: T, index: number) => R
): R[] {
  return items.map(renderItem);
}

// ============================================================================
// Development Utilities
// ============================================================================

// Logger hook for debugging
export function useLogger(name: string, value: any): void {
  useEffect(() => {
    console.log(`[${name}]`, value);
  }, [value]);
}

// Performance logger
export function usePerformance(componentName: string): void {
  const startTime = useRef(performance.now());

  useEffect(() => {
    return () => {
      const endTime = performance.now();
      const duration = endTime - startTime.current;
      console.log(`${componentName} rendered in ${duration.toFixed(2)}ms`);
    };
  }, []);
}

// ============================================================================
// API Integration Helpers
// ============================================================================

// Hook for fetching data
export function useFetch<T>(
  url: string,
  options?: RequestInit
): {
  data: T | null;
  loading: boolean;
  error: Error | null;
} {
  const [data, setData] = useState<T | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<Error | null>(null);

  useEffect(() => {
    let cancelled = false;

    fetch(url, options)
      .then(response => response.json())
      .then(json => {
        if (!cancelled) {
          setData(json);
          setLoading(false);
        }
      })
      .catch(err => {
        if (!cancelled) {
          setError(err);
          setLoading(false);
        }
      });

    return () => {
      cancelled = true;
    };
  }, [url]);

  return { data, loading, error };
}

// Hook for async operations
export function useAsync<T, E = Error>(
  asyncFunction: () => Promise<T>,
  deps?: any[]
): {
  result: T | null;
  loading: boolean;
  error: E | null;
} {
  const [result, setResult] = useState<T | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<E | null>(null);

  useEffect(() => {
    let cancelled = false;

    asyncFunction()
      .then(data => {
        if (!cancelled) {
          setResult(data);
          setLoading(false);
        }
      })
      .catch(err => {
        if (!cancelled) {
          setError(err);
          setLoading(false);
        }
      });

    return () => {
      cancelled = true;
    };
  }, deps || []);

  return { result, loading, error };
}

// ============================================================================
// Animation Helpers
// ============================================================================

// Use requestAnimationFrame hook
export function useAnimationFrame(callback: (timestamp: number) => void): void {
  const frameRef = useRef<number>();

  useEffect(() => {
    const animate = (timestamp: number) => {
      callback(timestamp);
      frameRef.current = requestAnimationFrame(animate);
    };

    frameRef.current = requestAnimationFrame(animate);

    return () => {
      if (frameRef.current) {
        cancelAnimationFrame(frameRef.current);
      }
    };
  }, []);
}

// ============================================================================
// State Management Helpers
// ============================================================================

// Simple state management with Context
export function createStore<T>(initialState: T): any {
  const StoreContext = createContext<{ state: T; setState: (state: T) => void }>({
    state: initialState,
    setState: () => {}
  });

  return {
    context: StoreContext,
    Provider: (props: any) => {
      const [state, setState] = useState(initialState);

      const React = getReact();
      return React.createElement(
        StoreContext.Provider,
        { value: { state, setState } },
        props.children
      );
    },
    useStore: () => useContext(StoreContext)
  };
}

// ============================================================================
// Type Exports for TypeScript Support
// ============================================================================

export type ReactElement = any;
export type ReactComponent = (props: any) => ReactElement;
export type ReactContext<T = any> = any;
