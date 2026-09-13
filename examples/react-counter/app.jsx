// React Counter Example
// This demonstrates a fully functional React application running in Artisan
// Uses the simplified art/react module for easy setup

import { setupReact, createRoot, useState, useEffect } from "art/react";

// Initialize React - just one line!
setupReact();

function Counter({ initialValue = 0, step = 1 }) {
  const [count, setCount] = useState(initialValue);
  const [history, setHistory] = useState([]);
  
  useEffect(() => {
    console.log("Counter initialized with value:", initialValue);
  }, [initialValue]);
  
  const increment = () => {
    const newCount = count + step;
    setCount(newCount);
    setHistory([...history, newCount]);
  };
  
  const decrement = () => {
    const newCount = count - step;
    setCount(newCount);
    setHistory([...history, newCount]);
  };
  
  const reset = () => {
    setCount(initialValue);
    setHistory([]);
  };
  
  return (
    <div style={{ border: "1px solid #ccc", padding: "20px", borderRadius: "8px" }}>
      <h2>Counter Application</h2>
      
      <div style={{ fontSize: "48px", fontWeight: "bold", textAlign: "center", margin: "20px 0" }}>
        {count}
      </div>
      
      <div style={{ display: "flex", gap: "10px", justifyContent: "center", marginBottom: "20px" }}>
        <button onclick={decrement} style={{ padding: "10px 20px", fontSize: "16px" }}>
          Decrease
        </button>
        <button onclick={reset} style={{ padding: "10px 20px", fontSize: "16px" }}>
          Reset
        </button>
        <button onclick={increment} style={{ padding: "10px 20px", fontSize: "16px" }}>
          Increase
        </button>
      </div>
      
      {history.length > 0 && (
        <div style={{ backgroundColor: "#f5f5f5", padding: "10px", borderRadius: "4px" }}>
          <h3>History</h3>
          <p>Changes: {history.length}</p>
          <p>Max: {Math.max(...history)}</p>
          <p>Min: {Math.min(...history)}</p>
          <p>Last 5: [{history.slice(-5).join(", ")}]</p>
        </div>
      )}
    </div>
  );
}

function App() {
  const [showStats, setShowStats] = useState(false);
  
  return (
    <div style={{ fontFamily: "system-ui, sans-serif", padding: "20px" }}>
      <h1>React in Artisan</h1>
      <p>Running React 18.3.1 through QuickJS</p>
      
      <Counter initialValue={0} step={1} />
      
      <hr style={{ margin: "40px 0" }} />
      
      <button onclick={() => setShowStats(!showStats)}>
        {showStats ? "Hide" : "Show"} Statistics
      </button>
      
      {showStats && (
        <div style={{ marginTop: "20px", padding: "15px", backgroundColor: "#e3f2fd", borderRadius: "4px" }}>
          <h3>Runtime Information</h3>
          <ul>
            <li>Framework: React 18.3.1</li>
            <li>Engine: QuickJS</li>
            <li>Rendering: Real DOM</li>
            <li>Platform: Artisan</li>
          </ul>
        </div>
      )}
    </div>
  );
}

// Mount the app to #root
const root = createRoot("root");
if (root) {
  root.render(<App />);
} else {
  console.error("Failed to create React root");
}
