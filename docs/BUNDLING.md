# Bundling NPM Packages for Artisan

This guide explains how to bundle npm packages (Angular, Express, Vue, etc.) for use within the Artisan QuickJS runtime.

## Quick Note: Built-in Packages

**Angular and RxJS are already built-in!** 

If you just need Angular, import it directly:

```typescript
import { setupAngular } from "art/angular";

setupAngular();
const Angular = require("@angular/core");
```

No bundling or setup needed. See [Angular integration example](../examples/bundling/angular/) for details.

This guide is for bundling **other npm packages** or custom versions.

## Overview

The module system can load pre-bundled npm packages by wrapping them in `registerModule()`. This guide shows how to:

1. Create a bundle from npm packages using esbuild
2. Wrap the bundle in a module registration function
3. Use the bundled module in your Artisan application

## Prerequisites

Before bundling, ensure you have:
- Node.js and npm installed
- esbuild installed: `npm install -g esbuild` or `npm install esbuild`
- The packages you want to bundle (e.g., `npm install express lodash`)

## General Bundling Process

### Step 1: Create a Bundle Entry Point

Create a file that exports everything you need:

```javascript
// bundle-angular.js
export * as core from "@angular/core";
export * as common from "@angular/common";
export * as forms from "@angular/forms";
```

### Step 2: Bundle with esbuild

```bash
esbuild bundle-angular.js \
  --bundle \
  --platform=neutral \
  --format=iife \
  --outfile=angular-bundle.js \
  --external:zone.js \
  --external:rxjs
```

**Key flags:**
- `--bundle`: Include all dependencies
- `--platform=neutral`: Not browser/node specific
- `--format=iife`: Wrap in immediately-invoked function
- `--external:*`: Mark certain modules as external (not bundled)

### Step 3: Create Module Registration

Create an ART file that loads the bundle:

```typescript
import { registerModule } from "art/modules";

export function registerAngularModule(): void {
  registerModule("@angular/core", function(module, exports, require) {
    // Include the bundled code here
    // This gets embedded at build time
    
    // Export what you need
    exports.Component = /* ... */;
    exports.NgModule = /* ... */;
    exports.Injectable = /* ... */;
  });
}
```

## Bundling React

React is already vendored at `third_party/react/`, but you can bundle additional React packages:

### Bundle React Packages

```bash
# Create entry point
cat > bundle-react.js << 'EOF'
export { default as React } from "react";
export { default as ReactDOM } from "react-dom";
export * from "react";
export * from "react-dom";
EOF

# Bundle
esbuild bundle-react.js \
  --bundle \
  --format=iife \
  --outfile=react-bundle.js \
  --external:react \
  --external:react-dom
```

### Use in Artisan

```typescript
import { registerModule } from "art/modules";

const reactBundleCode = readFile("react-bundle.js");

export function setupReactModules(): void {
  registerModule("react", function(m, e, r) {
    // Evaluated bundle provides exports
    eval(reactBundleCode);
    
    // React already in global scope from eval
    if (typeof React !== "undefined") {
      e.default = React;
      Object.assign(e, React);
    }
  });
  
  registerModule("react-dom", function(m, e, r) {
    if (typeof ReactDOM !== "undefined") {
      e.default = ReactDOM;
      Object.assign(e, ReactDOM);
    }
  });
}
```

## Bundling Angular

Angular is more complex due to its dependency on RxJS and Zone.js. Here's how to bundle it:

### Create Angular Bundle Entry

```javascript
// bundle-angular.js
export { default as angular } from "@angular/core";
export * from "@angular/core";
export * from "@angular/common";
export * from "@angular/platform-browser";
export * from "@angular/platform-browser-dynamic";
export * from "@angular/forms";
export * from "@angular/http";
```

### Bundle with External Dependencies

```bash
# Install dependencies
npm install @angular/core @angular/common @angular/platform-browser \
            @angular/platform-browser-dynamic @angular/forms \
            rxjs zone.js

# Bundle (excluding rxjs and zone.js - they need separate bundles)
esbuild bundle-angular.js \
  --bundle \
  --platform=neutral \
  --format=iife \
  --outfile=angular-bundle.js \
  --external:rxjs \
  --external:zone.js \
  --external:typescript
```

### Bundle RxJS Separately

```bash
cat > bundle-rxjs.js << 'EOF'
export * as rxjs from "rxjs";
export * from "rxjs";
export * from "rxjs/operators";
EOF

esbuild bundle-rxjs.js \
  --bundle \
  --format=iife \
  --outfile=rxjs-bundle.js
```

### Register Angular Modules

```typescript
import { registerModule } from "art/modules";

const angularBundleCode = readFile("angular-bundle.js");
const rxjsBundleCode = readFile("rxjs-bundle.js");

export function setupAngularModules(): void {
  // Register RxJS first (Angular depends on it)
  registerModule("rxjs", function(module, exports, require) {
    eval(rxjsBundleCode);
    if (typeof rxjs !== "undefined") {
      Object.assign(exports, rxjs);
    }
  });
  
  // Register Angular Core
  registerModule("@angular/core", function(module, exports, require) {
    // Make RxJS available to Angular
    globalThis.rxjs = require("rxjs");
    
    eval(angularBundleCode);
    
    if (typeof angular !== "undefined") {
      Object.assign(exports, angular);
    }
  });
  
  // Register other Angular packages
  registerModule("@angular/common", function(module, exports, require) {
    const core = require("@angular/core");
    // Re-export from core or add common-specific exports
    Object.assign(exports, core);
  });
}
```

## Complete Bundling Script

Save this as `bundle-packages.sh`:

```bash
#!/bin/bash
set -e

BUNDLE_DIR="./bundles"
mkdir -p "$BUNDLE_DIR"

echo "Installing dependencies..."
npm install @angular/core @angular/common @angular/forms \
            @angular/platform-browser @angular/platform-browser-dynamic \
            rxjs zone.js

echo "Bundling Angular..."
cat > /tmp/bundle-angular.js << 'EOF'
export * from "@angular/core";
export * from "@angular/common";
export * from "@angular/forms";
export * from "@angular/platform-browser";
export * from "@angular/platform-browser-dynamic";
EOF

esbuild /tmp/bundle-angular.js \
  --bundle \
  --format=iife \
  --outfile="$BUNDLE_DIR/angular.js" \
  --external:rxjs \
  --external:zone.js

echo "Bundling RxJS..."
cat > /tmp/bundle-rxjs.js << 'EOF'
export * from "rxjs";
export * from "rxjs/operators";
EOF

esbuild /tmp/bundle-rxjs.js \
  --bundle \
  --format=iife \
  --outfile="$BUNDLE_DIR/rxjs.js"

echo "Done! Bundles created in $BUNDLE_DIR/"
ls -lh "$BUNDLE_DIR/"
```

Run it:
```bash
chmod +x bundle-packages.sh
./bundle-packages.sh
```

## Wrapping Bundles Automatically

Create a wrapper generator script (Python):

```python
#!/usr/bin/env python3
import sys
import json

def create_module_wrapper(bundle_file, module_id):
    """Create ART code that wraps a bundle"""
    
    with open(bundle_file, 'r') as f:
        bundle_code = f.read()
    
    # Escape the code for embedding
    escaped = bundle_code.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')
    
    wrapper = f'''
import {{ registerModule }} from "art/modules";

export function register{module_id.replace("@", "").replace("/", "_")}(): void {{
  registerModule("{module_id}", function(module, exports, require) {{
    const code = "{escaped}";
    eval(code);
  }});
}}
'''
    
    return wrapper

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <bundle-file> <module-id>")
        sys.exit(1)
    
    bundle_file = sys.argv[1]
    module_id = sys.argv[2]
    
    wrapper = create_module_wrapper(bundle_file, module_id)
    print(wrapper)
```

Usage:
```bash
python3 wrap-bundle.py bundles/angular.js "@angular/core" > src/angular-module.ts
```

## Best Practices

### 1. **External Dependencies**
Mark large/external packages as external to avoid duplication:
```bash
esbuild input.js --bundle \
  --external:rxjs \
  --external:zone.js \
  --external:tslib
```

### 2. **Tree Shaking**
Let esbuild optimize the bundle:
```bash
esbuild input.js --bundle --minify \
  --drop:console \
  --drop:debugger
```

### 3. **Separate Core and Features**
Bundle core separately from optional features:
```javascript
// bundle-angular-core.js - just @angular/core
// bundle-angular-full.js - @angular/core + @angular/common + @angular/forms
```

### 4. **Version Management**
Lock bundle versions in package.json:
```json
{
  "dependencies": {
    "@angular/core": "^15.0.0",
    "rxjs": "^7.5.0"
  }
}
```

## Common Issues

### Issue: "RxJS is not defined"
**Solution**: Register RxJS module before modules that depend on it:
```typescript
setupRxJS();    // First
setupAngular(); // Second (depends on RxJS)
```

### Issue: Bundle is too large
**Solution**: 
- Use `--minify` flag
- Split into multiple smaller bundles
- Mark more dependencies as external
- Use `--drop:console` to remove debug code

### Issue: "Zone.js required"
**Solution**: 
- Bundle zone.js separately
- Register it before Angular modules
- Or make Angular use a polyfill version

## Examples

See the following for complete working examples:
- `examples/bundling/angular/` - Full Angular application
- `examples/bundling/react/` - Full React application
- `examples/bundling/custom/` - Custom npm package bundling

## Next Steps

1. Choose your target packages (Angular, React, Express, etc.)
2. Create bundle entry points
3. Run esbuild to create bundles
4. Generate module wrappers
5. Register modules in your app

For specific package examples, see the examples directory.
