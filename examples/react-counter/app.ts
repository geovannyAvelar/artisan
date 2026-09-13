// ART (TypeScript) example showing React integration
// This demonstrates calling React from ART code

import { createReactRoot, renderToRoot, getRootCount } from "art/react";
import { log } from "art/console";

// Example: ART code that manages React components
export function setupReactApplication(domRoot: any, appComponent: any): void {
  log("Setting up React application...");
  
  // Create a React root for rendering
  let reactContext = createReactRoot(domRoot, appComponent);
  log("React root created: " + reactContext);
  
  // Render the component
  let success = renderToRoot(reactContext, appComponent);
  if (success) {
    log("React component rendered successfully");
  } else {
    log("Failed to render React component");
  }
  
  // Log statistics
  let rootCount = getRootCount();
  log("Total React roots: " + rootCount);
}

// Example: ART function that processes data and passes it to React
export function processDataForReact(input: string): number {
  // In a real app, this might validate, transform, or fetch data
  let length = input.length;
  log("Processed input of length: " + length);
  return length;
}

// Example: ART function that responds to React events
export function handleReactEvent(eventType: string, eventData: string): void {
  log("React event received: " + eventType + " with data: " + eventData);
  // Could trigger further ART processing, state updates, etc.
}

// Example: Bridge between React and ART modules
export function bridgeReactAndART(): void {
  log("React-ART Bridge initialized");
  
  // In a full implementation:
  // 1. React components can call exposed ART functions
  // 2. ART code can create and manage React components
  // 3. Data flows bidirectionally between the two systems
}
