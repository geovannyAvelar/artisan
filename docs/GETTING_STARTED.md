# Getting Started with Artisan

Build native desktop applications from markup and code with Artisan.

## Installation

### Prerequisites

You need:
- **CMake** 3.20 or higher
- **C++20 compiler** (GCC 10+, Clang 11+, MSVC 2019+)
- **Python** 3.6+
- **Ninja** build system
- **pkg-config** packages: `freetype2`, `fontconfig`, `sdl2`
- **LLVM** 18 development libraries (`llvm-18-dev`)
- **Boehm GC** (`libgc-dev`)
- **Go** 1.21+ (for JSX compilation)

### System Setup

**Ubuntu/Debian:**
```bash
sudo apt-get install cmake ninja-build python3 pkg-config \
  libfreetype6-dev libfontconfig1-dev libsdl2-dev \
  llvm-18-dev libgc-dev golang-1.21
```

**macOS:**
```bash
brew install cmake ninja python pkg-config freetype fontconfig sdl2 \
  llvm@18 boehm-gc go
```

### Build Artisan

```bash
cd artisan
git submodule update --init --recursive
./build_skia.sh  # Build Skia (one-time, takes ~10 minutes)
cmake -S . -B build
cmake --build build --target artisan_cli
```

Put `build/artisan-cli` on your PATH, or use it with full path.

## Creating Your First App

### New Project

```bash
artisan-cli new hello-world
cd hello-world
```

This creates:
```
hello-world/
  pages/
    index.html          # Page markup
  app.tsx              # ART application code
```

### Building & Running

```bash
artisan-cli build hello-world --run
```

This:
1. Compiles ART code to machine code
2. Parses HTML into a widget tree
3. Links everything into a native binary
4. Runs the binary

**Output**: `./build/hello-world` (native executable, ~100MB)

### Project Structure

- **`pages/`** - HTML markup files (compiled into the binary)
- **`app.tsx`** - Main ART application (required if using ART)
- **`app.js`/`app.jsx`** - JavaScript application (optional)

See [Project Structure](PROJECT_STRUCTURE.md) for details.

## Choose Your Path

### Path 1: ART + Markup (Recommended for performance)

Use ART (compiled TypeScript-like language) for application logic:

```typescript
// app.tsx
import { Node, Event } from "art";

function onClick(event: Event): void {
  let button: Node = event.target;
  button.textContent = "Clicked!";
}

let button: Node = document.getElementById("my-button");
if (!button.isNull()) {
  button.addEventListener("click", onClick, false);
}
```

**Best for:**
- Performance-critical apps
- Native desktop applications
- Statically typed code

→ [ART Language Guide](ART_GUIDE.md)

### Path 2: JavaScript + React (Recommended for web developers)

Use JavaScript and React for the UI:

```javascript
// app.jsx
import React, { useState } from "react";

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

export default Counter;
```

React 18.3.1 is pre-included. No bundling needed.

**Best for:**
- React developers
- Rapid prototyping
- Component-based UIs

→ [JavaScript & React Guide](JAVASCRIPT_GUIDE.md)

### Path 3: Mixed (ART + React)

Combine both for best of both worlds:

```typescript
// app.tsx - ART code
import { Node } from "art";

export function setupArtUI(): void {
  // Build native UI with ART
  let container: Node = document.getElementById("art-root");
  // ...
}
```

```javascript
// app.jsx - React code
import React from "react";

function App() {
  // Build React UI
  return <div id="react-root">{/* ... */}</div>;
}
```

Both run simultaneously, sharing the same DOM.

## Common Tasks

### Add a New Page

```bash
touch my-app/pages/settings.html
```

In `settings.html`:
```html
<div id="root">
  <h1>Settings</h1>
  <!-- content -->
</div>
```

Link to it from other pages:
```html
<a href="/settings">Go to Settings</a>
```

### Use React

No extra setup needed! React is pre-vendored.

```javascript
// app.jsx
import React from "react";
import ReactDOM from "react-dom";

function App() {
  return <h1>Hello React!</h1>;
}

ReactDOM.createRoot(document.getElementById("root")).render(<App />);
```

→ [Full React Guide](JAVASCRIPT_GUIDE.md)

### Use npm Packages

Bundle any npm package (Angular, Express, etc.) and use it:

```bash
cd my-app
npm install @angular/core
# Use esbuild to bundle
esbuild --bundle # ...
```

→ [Bundling Guide](BUNDLING.md)

### Organize Code

Use the module system to split your code:

```typescript
// modules/math.ts
export function add(a: number, b: number): number {
  return a + b;
}

// app.ts
import { add } from "./modules/math";
let result: number = add(5, 3); // 8
```

→ [Module System Guide](MODULES.md)

## Debugging

### Build Errors

If compilation fails:

1. Check CMake configuration: `cat build/CMakeCache.txt`
2. Rebuild from scratch: `rm -rf build && cmake -S . -B build`
3. Check compiler availability: `gcc --version`, `llvm-config-18 --version`

### Runtime Issues

If the app crashes at runtime:

1. Check console output for error messages
2. Build with debug symbols: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
3. Run with debugger: `gdb ./build/my-app`

### Performance

Check if your app is CPU-bound or I/O-bound:
- ART code is pre-compiled → native speed
- JavaScript runs in QuickJS → slower but acceptable for UI
- DOM operations go directly to native code → fast

## Next Steps

1. **Learn ART**: [ART Language Guide](ART_GUIDE.md)
2. **Learn JavaScript/React**: [JavaScript Guide](JAVASCRIPT_GUIDE.md)
3. **Explore Examples**: [example projects](../examples/)
4. **Build Something**: Create your first app!

## Common Issues

### "artisan-cli: command not found"

Add to PATH:
```bash
export PATH="$PATH:$(pwd)/build"
```

### "Could not find LLVM 18"

Install LLVM 18:
```bash
# Ubuntu
sudo apt-get install llvm-18-dev

# macOS
brew install llvm@18
```

### "libgc not found"

Install Boehm GC:
```bash
# Ubuntu
sudo apt-get install libgc-dev

# macOS
brew install boehm-gc
```

## Getting Help

- **Documentation**: Read the [guides](.)
- **Examples**: Check [example projects](../examples/)
- **Issues**: Report on [GitHub](https://github.com/geovannyAvelar/artisan/issues)

---

**Ready?** Pick your path above and start building!
