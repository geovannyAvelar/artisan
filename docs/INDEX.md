# Artisan Documentation Index

Complete documentation for the Artisan framework.

## Getting Started

**New to Artisan?** Start here:

1. **[Getting Started](GETTING_STARTED.md)** (20 min read)
   - Installation and setup
   - Creating your first project
   - Running and building apps
   - Choosing between ART and JavaScript

2. **[Project Structure](PROJECT_STRUCTURE.md)** (10 min read)
   - Understanding project layout
   - File organization
   - Multi-file projects
   - Static assets

## Development Guides

Choose your preferred language:

### Using ART (Compiled Language)

**[ART Language Guide](ART_GUIDE.md)** - Complete reference for ART
- Language features (variables, functions, interfaces, generics)
- DOM API (elements, attributes, classes, styles, events)
- Event handling and timers
- JSX support
- Performance tips and patterns

**Best for:** High performance, native apps, statically typed code

### Using JavaScript & React

**[JavaScript & React Guide](JAVASCRIPT_GUIDE.md)** - Using JS and React 18.3.1
- React hooks (useState, useEffect, useCallback, useMemo, useRef)
- Component patterns
- State management (Context API, custom hooks)
- Styling and forms
- Performance optimization
- Debugging and common pitfalls

**Best for:** Web developers, rapid prototyping, component reuse

## Organizing Code

**[Module System Guide](MODULES.md)** - CommonJS and ES6 modules
- Module registration and loading
- CommonJS require() patterns
- ES6 import/export syntax
- Module caching and dependencies
- Best practices for organizing code

**Use when:** Building large applications, splitting code into logical units

## Using Frameworks & Libraries

**[Bundling Guide](BUNDLING.md)** - Using npm packages
- Bundling Angular, React, Vue, and other frameworks
- esbuild integration
- Creating bundles for npm packages
- Wrapping bundles for Artisan

**Use when:** You want to bring in Angular, Express, or other npm packages

## Advanced Topics

### Integration & Architecture

**[Module System Architecture](MODULE_SYSTEM_SUMMARY.md)** - Architecture overview
- How the module system enables frameworks
- Performance considerations
- Bundling strategy
- Next steps for npm integration

**[React Features](REACT_IN_ARTISAN.md)** - Complete React feature overview
- Component architecture
- Hooks support
- Performance notes
- Integration patterns

**[React Patterns](REACT_INTEGRATION.md)** - Detailed React patterns
- Creating React components
- Using Artisan-specific features
- Styling and theming
- Advanced patterns

## Complete Examples

See `examples/` directory for working projects:

### React Counter App
**[examples/react-counter/](../examples/react-counter/)**
- React functional components
- useState hook
- Event handling
- State management
- Complete working example

### Multi-Module Application
**[examples/module-system/](../examples/module-system/)**
- Service-oriented architecture
- Dependency injection
- Module organization
- Design patterns
- Real-world multi-module app

### Angular Bundling
**[examples/bundling/angular/](../examples/bundling/angular/)**
- How to bundle Angular
- RxJS integration
- Angular decorators and features
- Complete setup guide

### React Bundling
**[examples/bundling/react/](../examples/bundling/react/)**
- Bundling React packages
- React Router, Redux examples
- Pre-vendored React usage
- Pattern demonstrations

## Quick Reference

### Choosing Your Tech Stack

| Need | Use | Reason |
|------|-----|--------|
| High performance | ART | Compiled to machine code |
| Simple static UI | ART | Built-in DOM API, small binary |
| Component-based UI | React | Familiar, large ecosystem |
| Complex state | React + Context | Good state management |
| Native desktop app | ART | Full native integration |
| Web developer experience | React | Familiar syntax and patterns |

### Common Tasks

#### Create a new project
```bash
artisan-cli new my-app
cd my-app
artisan-cli build . --run
```

#### Add a React component
```javascript
// Create app.jsx
import React, { useState } from "react";
import ReactDOM from "react-dom/client";

function App() {
  const [count, setCount] = useState(0);
  return <button onClick={() => setCount(count + 1)}>Count: {count}</button>;
}

ReactDOM.createRoot(document.getElementById("root")).render(<App />);
```

#### Use a module
```typescript
// Create lib/math.ts
export function add(a: number, b: number): number {
  return a + b;
}

// Use in app.ts
import { add } from "./lib/math";
```

#### Bundle an npm package
```bash
npm install @angular/core
esbuild --bundle entry.js --outfile=bundle.js
# Wrap with Python script (see BUNDLING.md)
```

## Documentation Map

```
docs/
  ├── INDEX.md                    (this file)
  ├── GETTING_STARTED.md          Setup & first app
  ├── PROJECT_STRUCTURE.md        Project layout
  ├── ART_GUIDE.md               ART language
  ├── JAVASCRIPT_GUIDE.md        JavaScript & React
  │
  ├── ../README.md               Project overview
  ├── ../MODULES.md              Module system
  ├── ../BUNDLING.md             npm package bundling
  │
  ├── ../MODULE_SYSTEM_SUMMARY.md    Architecture notes
  ├── ../REACT_IN_ARTISAN.md         React feature overview
  ├── ../REACT_INTEGRATION.md        React detailed patterns
  │
  └── ../examples/               Working examples
      ├── react-counter/
      ├── module-system/
      └── bundling/
          ├── angular/
          └── react/
```

## Learning Path

### Beginner (2-3 hours)
1. [Getting Started](GETTING_STARTED.md) - 20 min
2. [Project Structure](PROJECT_STRUCTURE.md) - 10 min
3. Choose your path:
   - ART: [ART Guide](ART_GUIDE.md) (30 min)
   - React: [JavaScript Guide](JAVASCRIPT_GUIDE.md) (30 min)
4. Build something simple

### Intermediate (1-2 hours)
1. [Module System](../MODULES.md) - 20 min
2. Organize multi-file project
3. Study working examples (15-30 min each)

### Advanced (2-3 hours)
1. [Bundling Guide](../BUNDLING.md) - 30 min
2. Bundle your first npm package
3. [Integration Guides](../REACT_IN_ARTISAN.md) - 30 min
4. Mix ART and React in one project

## FAQ

**Q: Do I need Node.js to use Artisan?**
A: No. The CLI is a single executable. You only need Node if you want to use npm packages.

**Q: Is React pre-installed?**
A: Yes! React 18.3.1 is vendored and ready to use.

**Q: Can I use both ART and JavaScript in one app?**
A: Yes. They can coexist and share the DOM.

**Q: How do I bundle npm packages?**
A: See [Bundling Guide](../BUNDLING.md) for step-by-step instructions.

**Q: What's the difference between ART and JavaScript performance?**
A: ART compiles to machine code (native speed). JavaScript runs in QuickJS interpreter (slower but still acceptable for UI).

**Q: How do I debug?**
A: Use console.log in both ART and JavaScript. Use gdb for native debugging.

## Resources

- **[GitHub Repository](https://github.com/geovannyAvelar/artisan)**
- **[Source Code](../art/)**
- **[Examples](../examples/)**

## Getting Help

- Read the relevant guide (above)
- Check working examples
- Review source code in `art/`
- [Report issues on GitHub](https://github.com/geovannyAvelar/artisan/issues)

---

**Ready to get started?** → [Getting Started](GETTING_STARTED.md)
