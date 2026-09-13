# Bundling Angular for Artisan

This example demonstrates how to bundle Angular and use it within the Artisan QuickJS runtime.

## What This Example Shows

- Setting up Angular for bundling with esbuild
- Handling Angular's dependencies (RxJS, Zone.js)
- Creating module wrappers for Angular packages
- Using Angular in Artisan applications

## Quick Start

### 1. Install Dependencies

```bash
cd examples/bundling/angular
npm install @angular/core @angular/common @angular/forms \
            @angular/platform-browser @angular/platform-browser-dynamic \
            rxjs zone.js typescript
```

### 2. Bundle Angular

```bash
# Install esbuild if not already installed
npm install esbuild --save-dev

# Run bundling script
bash bundle.sh
```

This creates:
- `bundles/rxjs.js` - RxJS library
- `bundles/angular-core.js` - Angular core
- `bundles/angular-full.js` - Angular + common + forms

### 3. Generate Module Wrappers

```bash
python3 ../wrap-bundle.py bundles/rxjs.js "rxjs" > src/rxjs-module.ts
python3 ../wrap-bundle.py bundles/angular-core.js "@angular/core" > src/angular-module.ts
```

### 4. Use in Your Application

```typescript
import { setupAngularModules } from "./src/angular-module";
import { setupRxJS } from "./src/rxjs-module";

export function startApp(): void {
  // Register modules
  setupRxJS();
  setupAngularModules();
  
  // Use Angular
  const Angular = require("@angular/core");
  
  // Create components
  const HelloComponent = Angular.Component({
    selector: 'app-hello',
    template: '<h1>Hello from Angular!</h1>'
  })(class HelloComponent {});
  
  // Use Angular decorators and features
  console.log("Angular loaded:", typeof Angular.Component === 'function');
}
```

## Directory Structure

```
examples/bundling/angular/
  ├── README.md                    # This file
  ├── bundle.sh                    # Script to bundle Angular
  ├── bundle-*.js                  # Entry points for esbuild
  ├── bundles/                     # Output bundles
  │   ├── rxjs.js
  │   ├── angular-core.js
  │   └── angular-full.js
  ├── src/
  │   ├── app.ts                   # Main application
  │   ├── angular-module.ts        # Angular module registration
  │   └── rxjs-module.ts           # RxJS module registration
  └── package.json
```

## Key Files

### `bundle.sh`
Bash script that:
1. Creates bundle entry points
2. Runs esbuild to create bundles
3. Lists output files

### `app.ts`
Shows how to:
- Register bundled modules
- Import Angular from the module system
- Use Angular features

### `src/angular-module.ts` (Generated)
Auto-generated module wrapper that:
- Embeds the bundled Angular code
- Registers it with `registerModule()`
- Exports Angular components and decorators

## Bundling Process Explained

### Why Separate Bundles?

**RxJS (standalone)**
- Angular depends on RxJS
- Must be bundled separately
- Large library that could be shared

**Angular Core**
- Just @angular/core
- Minimal but functional
- Good for small applications

**Angular Full**
- @angular/core + @angular/common + @angular/forms
- Complete Angular experience
- Larger bundle

### esbuild Flags Used

```bash
esbuild input.js \
  --bundle              # Include all dependencies
  --format=iife         # Wrap in function for isolation
  --platform=neutral    # Not browser/node specific
  --minify              # Reduce size
  --external:zone.js    # Don't bundle zone.js
```

## Size Considerations

Typical bundle sizes:
- RxJS alone: ~500KB
- Angular core: ~1.5MB
- Angular full: ~2MB
- Minified and gzipped: ~60-70% smaller

## Using Angular Features

Once bundled and registered, you can use:

### Decorators
```typescript
const Angular = require("@angular/core");

class MyComponent {
  name = "World";
}

const Decorated = Angular.Component({
  selector: 'my-component',
  template: '<h1>Hello {{name}}</h1>'
})(MyComponent);
```

### Dependency Injection
```typescript
class DataService {
  getData() { return ["a", "b", "c"]; }
}

const Component = Angular.Component({
  selector: 'data-component'
})(class {
  constructor(service: DataService) {
    this.data = service.getData();
  }
});
```

### Reactive Forms
```typescript
const FormBuilder = require("@angular/forms").FormBuilder;

class MyForm {
  form: any;
  
  constructor(fb: FormBuilder) {
    this.form = fb.group({
      name: [''],
      email: ['']
    });
  }
}
```

## Common Issues and Solutions

### "RxJS is not defined"
**Problem**: Angular module tries to use RxJS before it's registered
**Solution**: Call `setupRxJS()` before `setupAngularModules()`

### "Zone.js is required"
**Problem**: Angular expects zone.js polyfills
**Solution**: Either bundle zone.js or provide minimal polyfill:
```typescript
globalThis.Zone = {
  current: { run: (fn: any) => fn() }
};
```

### Bundle too large
**Solution**: 
- Use `--minify` flag in esbuild
- Bundle only what you need
- Consider using lighter alternatives

### "Cannot find module"
**Problem**: Bundled code references external module
**Solution**: Mark it as external during bundling:
```bash
esbuild input.js --bundle --external:tslib --external:zone.js
```

## Advanced: Custom Angular Bundle

To bundle only specific Angular features:

```javascript
// bundle-custom-angular.js
// Only what your app needs
export { Component, NgModule } from "@angular/core";
export { CommonModule } from "@angular/common";
export { FormsModule } from "@angular/forms";
```

Then bundle with:
```bash
esbuild bundle-custom-angular.js --bundle --minify
```

This creates a much smaller bundle with only the features you use.

## Performance Tips

1. **Lazy load modules**: Register only the modules you need
2. **Tree-shake**: Use esbuild's tree shaking to remove unused code
3. **Separate concerns**: Bundle different packages separately
4. **Cache modules**: The module system caches, so first load is worst
5. **Monitor sizes**: Keep bundles under 2MB when possible

## References

- Angular Documentation: https://angular.io/docs
- esbuild Bundler: https://esbuild.github.io/
- Artisan Module System: See MODULES.md
- Bundling Guide: See BUNDLING.md

## Next Steps

1. Try the bundling process step by step
2. Experiment with different bundle configurations
3. Create a bundling pipeline for your project
4. Integrate with your build system
