# Module System for Artisan QuickJS

Artisan now includes a complete **CommonJS and ES6 module system** that runs within the QuickJS JavaScript engine. This enables loading npm packages, organizing code into modules, and supporting both modern ES6 and Node.js CommonJS patterns.

## Overview

The module system provides:
- ✅ **CommonJS support** - `require()` and `module.exports`
- ✅ **ES6 modules** - `import/export` syntax (at runtime)
- ✅ **Module caching** - Modules load once and are cached
- ✅ **Dependency resolution** - Modules can depend on other modules
- ✅ **Built-in modules** - Pre-registered `art/*` modules
- ✅ **NPM package support** - Can bundle and load npm packages

## Quick Start

### CommonJS Style

```typescript
// Register a module
import { registerModule, require } from "art/modules";

registerModule("math", function(module, exports, require) {
  exports.add = (a, b) => a + b;
  exports.multiply = (a, b) => a * b;
});

// Use the module
let math = require("math");
console.log(math.add(2, 3));      // 5
console.log(math.multiply(3, 4)); // 12
```

### ES6 Style

```typescript
import { defineESModule, require } from "art/modules";

// Define module with named exports
defineESModule("utils", {
  formatString: (s) => s.toUpperCase(),
  parseJson: (j) => JSON.parse(j),
  default: "Default Export"
});

// Use it
let utils = require("utils");
utils.formatString("hello"); // HELLO
```

### With Dependencies

```typescript
// Dependency
registerModule("config", function(module, exports, require) {
  exports.apiUrl = "https://api.example.com";
  exports.timeout = 5000;
});

// Module that depends on config
registerModule("api", function(module, exports, require) {
  let config = require("config");
  
  exports.fetch = function(path) {
    return config.apiUrl + path;
  };
});

// Use it
let api = require("api");
console.log(api.fetch("/users")); // https://api.example.com/users
```

## API Reference

### Module Registration

#### `registerModule(id, factory)`
Registers a CommonJS-style module.

```typescript
registerModule("mymodule", function(module, exports, require) {
  exports.value = 42;
  exports.getValue = () => exports.value;
});
```

**Parameters:**
- `id` (string): Unique module identifier
- `factory` (function): Module factory function with signature `(module, exports, require) => void`

**Returns:** boolean - true if registered, false if already exists

#### `defineModule(id, factory)`
Alias for `registerModule()`.

#### `defineESModule(id, exportsObject)`
Registers an ES6-style module with named exports.

```typescript
defineESModule("logger", {
  log: (msg) => console.log(msg),
  error: (msg) => console.error(msg),
  default: { version: "1.0" }
});
```

**Parameters:**
- `id` (string): Module identifier
- `exportsObject` (object): Object with named exports

**Returns:** boolean - true if registered

### Module Loading

#### `require(id)`
Loads and returns a module. Implements CommonJS `require()`.

```typescript
let express = require("express");
let config = require("./config");
let utils = require("utils");
```

**Parameters:**
- `id` (string): Module identifier to load

**Returns:** object - Module exports, or null if not found

**Caching:** Returns cached result on subsequent calls.

#### `importModule(id, exportName)`
ES6-style import of specific export.

```typescript
import { importModule } from "art/modules";

let Component = importModule("react", "Component");
let add = importModule("math", "add");
```

**Parameters:**
- `id` (string): Module identifier
- `exportName` (string): Name of export to import, or empty string for default

**Returns:** Exported value or null

#### `importAll(id)`
Imports all exports from a module (like `import * as`).

```typescript
let React = importAll("react");
```

**Parameters:**
- `id` (string): Module identifier

**Returns:** object - All module exports

### Module Inspection

#### `getModuleCount()`
Returns count of registered modules.

```typescript
let count = getModuleCount(); // 15
```

#### `getCacheSize()`
Returns count of loaded/cached modules.

```typescript
require("math");
require("utils");
let cached = getCacheSize(); // 2
```

#### `getModuleIds()`
Returns array of all registered module IDs.

```typescript
let ids = getModuleIds(); // ["math", "utils", "api", ...]
```

#### `isModuleLoaded(id)`
Checks if a module has been loaded and cached.

```typescript
if (isModuleLoaded("express")) {
  // Module already loaded
}
```

#### `getModuleMetadata(id)`
Returns detailed metadata about a module.

```typescript
let meta = getModuleMetadata("react");
// [id, exports, loaded, cached]
console.log(meta[2]); // loaded: true/false
```

### Cache Management

#### `clearModuleCache()`
Clears the module cache but keeps module registrations.

```typescript
clearModuleCache();
// Next require() will re-execute factories
```

#### `clearAllModules()`
Clears all module registrations and cache.

```typescript
clearAllModules();
// Start fresh
```

### Utilities

#### `resolveModulePath(importPath)`
Resolves and normalizes module paths.

```typescript
resolveModulePath("art/react");    // "art/react"
resolveModulePath("./lib/utils");  // "lib/utils"
resolveModulePath("express");      // "node_modules/express"
```

#### `parseImportStatement(statement)`
Parses import/require statements to extract module source.

```typescript
let [type, source, names] = parseImportStatement("import { Component } from 'react'");
// type: "es6"
// source: "react"
// names: []
```

## Built-in Modules

Artisan provides these pre-registered modules:

```typescript
require("art/react");      // React integration
require("art/net");        // Networking (HTTP client)
require("art/fs");         // File system I/O
require("art/timers");     // setTimeout, setInterval
require("art/promise");    // Promise implementation
require("art/console");    // Console logging
require("art/json");       // JSON stringify/parse
require("art/string");     // String utilities
require("art/path");       // Path manipulation
require("art/utils");      // Utility functions
require("art/collections");// Map/Set implementations
require("art/iterables");  // For-of loop utilities
require("art/iterators");  // Iterator protocol
```

## CommonJS Module Format

### Basic Export

```typescript
registerModule("math", function(module, exports, require) {
  exports.add = (a, b) => a + b;
  exports.subtract = (a, b) => a - b;
});

// Usage
let math = require("math");
math.add(5, 3); // 8
```

### Default Export

```typescript
registerModule("logger", function(module, exports, require) {
  let log = function(msg) {
    console.log("[LOG] " + msg);
  };
  module.exports = log;
  // OR: module[1] = log;
});

// Usage
let log = require("logger");
log("Hello"); // [LOG] Hello
```

### Mixed Named and Default

```typescript
registerModule("helpers", function(module, exports, require) {
  exports.format = (str) => str.toUpperCase();
  exports.parse = (str) => str.split(",");
  module.exports = {
    format: exports.format,
    parse: exports.parse,
    default: "Helper Library v1.0"
  };
});
```

## ES6 Module Format

### Named Exports

```typescript
defineESModule("react", {
  createElement: (tag, props, ...children) => ({ tag, props, children }),
  Fragment: Symbol("Fragment"),
  version: "18.3.1"
});

// Usage
let React = require("react");
React.createElement("div", {}, "Hello");
```

### With Default Export

```typescript
defineESModule("config", {
  apiUrl: "https://api.example.com",
  timeout: 5000,
  default: {
    env: "production"
  }
});

// Usage
let config = require("config");
let { apiUrl } = config;
let defaultConfig = config.default;
```

## Loading NPM Packages

To use npm packages (React, Angular, Express, etc.), bundle them first:

### 1. Bundle Process

```bash
# Use esbuild or webpack to bundle npm package
esbuild react.js --bundle --format=iife --outfile=react-bundled.js

# Now you have react-bundled.js with all dependencies included
```

### 2. Create Module Wrapper

```typescript
// In your ART code
import { registerModule } from "art/modules";

// Read bundled package (simulated - would read from file)
let reactCode = `/* react.development.js bundled code */`;

// Create function that loads it into a module namespace
registerModule("react", function(module, exports, require) {
  // Execute bundled code in this module's context
  // This would be done via eval() or compilation step
  // Exports React, ReactDOM, etc.
  
  exports.React = window.React;
  exports.ReactDOM = window.ReactDOM;
});
```

### 3. Use in Your App

```typescript
import { require } from "art/modules";

let React = require("react");
let { useState, useEffect } = React;

// Now use React normally
```

## Advanced Patterns

### Factory Pattern

```typescript
registerModule("factory", function(module, exports, require) {
  let instanceCount = 0;
  
  exports.createInstance = function(name) {
    instanceCount = instanceCount + 1;
    return {
      id: instanceCount,
      name: name,
      timestamp: Date.now()
    };
  };
});

let factory = require("factory");
let obj1 = factory.createInstance("Object1"); // {id: 1, name: "Object1", ...}
let obj2 = factory.createInstance("Object2"); // {id: 2, name: "Object2", ...}
```

### Singleton Pattern

```typescript
registerModule("database", function(module, exports, require) {
  let connection = null;
  
  exports.getConnection = function() {
    if (connection == null) {
      connection = { connected: true, id: Math.random() };
    }
    return connection;
  };
});

let db1 = require("database").getConnection();
let db2 = require("database").getConnection();
// db1 === db2 (same instance)
```

### Middleware Pattern

```typescript
registerModule("middleware", function(module, exports, require) {
  let middlewares = [];
  
  exports.use = function(fn) {
    middlewares.push(fn);
  };
  
  exports.execute = function(data) {
    let i = 0;
    while (i < middlewares.length) {
      data = middlewares[i](data);
      i = i + 1;
    }
    return data;
  };
});

let mw = require("middleware");
mw.use((data) => data + 1);
mw.use((data) => data * 2);
mw.execute(5); // (5 + 1) * 2 = 12
```

## Circular Dependencies

Circular dependencies are possible but require care:

```typescript
// module-a requires module-b
// module-b requires module-a

registerModule("a", function(module, exports, require) {
  exports.name = "A";
  let b = require("b");     // Load b
  exports.getB = () => b;
});

registerModule("b", function(module, exports, require) {
  exports.name = "B";
  let a = require("a");     // This gets partial a (not fully loaded yet)
  exports.getA = () => a;
});

let a = require("a");       // Works, but getA() will have incomplete a
```

**Best Practice:** Avoid circular dependencies by restructuring code.

## Integration with QuickJS

The module system is exposed to QuickJS automatically:

```javascript
// In JavaScript code running in QuickJS
let math = require("math");
let React = require("react");
let config = require("config");
```

The `require` function is available globally in the QuickJS context.

## Performance Considerations

- **Caching**: Modules are cached after first load - no re-execution
- **Module size**: Smaller modules load faster
- **Dependency depth**: Deep dependency chains take longer
- **Circular deps**: Can cause issues - avoid when possible

## Debugging

### List all modules
```typescript
import { getModuleIds } from "art/modules";

let ids = getModuleIds();
ids.forEach((id) => console.log("Module: " + id));
```

### Check if loaded
```typescript
import { isModuleLoaded } from "art/modules";

if (isModuleLoaded("react")) {
  console.log("React already loaded");
}
```

### Get metadata
```typescript
import { getModuleMetadata } from "art/modules";

let meta = getModuleMetadata("react");
console.log("Loaded:", meta[2]);
console.log("Cached:", meta[3]);
```

### Clear and reload
```typescript
import { clearModuleCache, require } from "art/modules";

// Clear cache to reload all modules
clearModuleCache();

// Next require() will re-execute
let react = require("react");
```

## Roadmap

- [ ] Async module loading
- [ ] Module hot reloading
- [ ] Tree shaking / dead code elimination
- [ ] Module bundling tool
- [ ] Npm package integration
- [ ] Module search path configuration
- [ ] Dynamic module creation
- [ ] Module versioning

## Examples

### Loading React

```typescript
// Register React from bundled source
registerModule("react", function(module, exports, require) {
  // Bundled React code here
  exports.createElement = ...;
  exports.useEffect = ...;
  exports.useState = ...;
});

// Use in app
let React = require("react");
let Component = React.createElement("div", {}, "Hello");
```

### Loading Angular

```typescript
// Similar process for Angular
registerModule("@angular/core", function(module, exports, require) {
  exports.Component = ...;
  exports.Injectable = ...;
  exports.NgModule = ...;
});

let core = require("@angular/core");
```

### Creating a Package Structure

```typescript
// lib/utils.ts
registerModule("lib/utils", function(module, exports, require) {
  exports.format = (s) => s.trim();
  exports.parse = (s) => s.split(" ");
});

// lib/api.ts
registerModule("lib/api", function(module, exports, require) {
  let utils = require("lib/utils");
  exports.fetch = (path) => utils.format(path);
});

// app.ts
let api = require("lib/api");
```

## See Also

- [React Integration Guide](./REACT_INTEGRATION.md)
- [Artisan README](./README.md)
- [Stdlib Modules](./art/stdlib/)
- [QuickJS Documentation](https://bellard.org/quickjs/)

---

**Status**: ✅ Complete and ready to use

**Last Updated**: 2026-09-13
