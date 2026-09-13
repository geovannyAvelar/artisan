# ARTISAN - Native Apps from Markup

**A**daptive **R**untime for **T**ranslation of **I**nterpreted **S**cripts to **A**pplications **N**atively

A framework for building native desktop apps from HTML-like markup. Markup is compiled ahead-of-time into a widget tree baked straight into the binary, and rendered with Skia. App behavior comes from ART (a statically typed language compiled to native code) and JavaScript/JSX (interpreted via QuickJS), both driving the same mutable DOM at runtime.

## Quick Start

### Prerequisites

- CMake 3.20+, C++20 compiler, Python 3, Ninja
- `pkg-config` packages: `freetype2`, `fontconfig`, `sdl2`
- LLVM 18 and Boehm-Demers-Weiser GC (`libgc-dev`)
- Go 1.21+ (for JSX compilation)

### Build

```bash
git submodule update --init --recursive
./build_skia.sh
cmake -S . -B build
cmake --build build --target artisan_cli
```

### Create a Project

```bash
artisan-cli new my-app
artisan-cli build my-app --run
```

This scaffolds a project with ART (`app.tsx`) and creates a native binary.

## Documentation

### Getting Started
- **[Getting Started](docs/GETTING_STARTED.md)** - Setup, project structure, building
- **[Project Structure](docs/PROJECT_STRUCTURE.md)** - Understanding the project layout

### Development Guides
- **[ART Language Guide](docs/ART_GUIDE.md)** - ART language features, DOM API, compilation
- **[JavaScript & React](docs/JAVASCRIPT_GUIDE.md)** - Using JavaScript, React 18.3.1, QuickJS runtime
- **[Module System](docs/MODULES.md)** - Organizing code with CommonJS and ES6 modules
- **[Bundling NPM Packages](docs/BUNDLING.md)** - Bundling Angular, React, and other npm packages

### Practical Examples
- **[React Counter App](examples/react-counter/)** - Complete React application example
- **[Module System Example](examples/module-system/)** - Multi-module application patterns
- **[Angular Bundling](examples/bundling/angular/)** - How to bundle and use Angular
- **[React Bundling](examples/bundling/react/)** - Bundling React packages

### Advanced Topics
- **[Module System Architecture](docs/MODULE_SYSTEM_SUMMARY.md)** - Architecture and capabilities
- **[React Features](docs/REACT_IN_ARTISAN.md)** - Complete React support overview
- **[React Patterns](docs/REACT_INTEGRATION.md)** - Detailed React usage patterns

## Features

✅ **Native Performance** - Compiled to machine code via LLVM  
✅ **Two Languages** - ART (compiled) + JavaScript/JSX (interpreted)  
✅ **Single Binary** - No external dependencies, no runtime  
✅ **React Support** - Pre-vendored React 18.3.1 with all hooks  
✅ **Module System** - CommonJS and ES6 modules  
✅ **NPM Packages** - Bundle and use Angular, Express, and other frameworks  
✅ **Hot Development** - Fast rebuilds with incremental compilation  
✅ **Full DOM API** - querySelector, events, classList, style, and more  

## Architecture

```
┌─────────────────────────────────┐
│  HTML Markup (pages/*.html)    │ ─┐
│  ART Code (app.tsx)             │  │ Compile time
│  JavaScript (app.js/app.jsx)   │ ─┘
│
├─────────────────────────────────┤
│  LLVM Compiler (ART)            │
│  JSX Transformer                │
│  HTML Parser                    │
├─────────────────────────────────┤
│  Native Binary (Skia renderer)  │
│
├─────────────────────────────────┤
│  Runtime (QuickJS VM + DOM API) │ Runtime
│  Timer Queue, Event Loop        │
│  Animation Frame Queue          │
└─────────────────────────────────┘
```

## What's Included

- **ART Compiler** - Statically typed language compiled to native code
- **JSX Transformer** - JSX syntax support for both ART and JavaScript
- **QuickJS Engine** - Lightweight JavaScript interpreter
- **Skia Renderer** - High-quality 2D graphics
- **DOM Bridge** - Native API for DOM manipulation
- **Module System** - CommonJS and ES6 module support
- **Standard Library** - 30+ utility modules for common tasks

## Learning Resources

1. **Start here**: [Getting Started Guide](docs/GETTING_STARTED.md)
2. **Choose your path**:
   - Prefer markup & ART? → [ART Language Guide](docs/ART_GUIDE.md)
   - Prefer JavaScript & React? → [JavaScript Guide](docs/JAVASCRIPT_GUIDE.md)
3. **Organize your code**: [Module System](docs/MODULES.md)
4. **Bring in frameworks**: [Bundling Guide](docs/BUNDLING.md)

## Development

### Building from source

```bash
# Full build
cmake -S . -B build
cmake --build build

# Build specific targets
cmake --build build --target artisan_cli
cmake --build build --target artisan_tests
cmake --build build --target art_compiler
```

### Running tests

```bash
cmake --build build --target artisan_tests
./build/artisan_tests
```

## Links

- **[Issues & Discussions](https://github.com/geovannyAvelar/artisan/issues)**
- **[Examples](examples/)** - Sample projects and patterns
- **[Source Code](art/)** - ART compiler, standard library, tests

## License

MIT License - See LICENSE file for details

---

**Ready to get started?** → [Getting Started Guide](docs/GETTING_STARTED.md)
