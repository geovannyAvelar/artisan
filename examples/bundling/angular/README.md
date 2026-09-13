# Using Angular in Artisan

Angular and RxJS are **built-in** to Artisan! No bundling or setup required.

## Quick Start (Recommended)

Use the built-in Angular module:

```typescript
import { setupAngular, createFormGroup, getService } from "art/angular";

export function startApp(): void {
  // Initialize Angular - one line!
  setupAngular();
  
  // Use Angular immediately
  const Angular = require("@angular/core");
  const Forms = require("@angular/forms");
  
  // Create components
  @Angular.Component({
    selector: "app-hello",
    template: "<h1>Hello from Angular!</h1>"
  })
  class HelloComponent {}
  
  // Create forms
  const form = createFormGroup({
    name: [""],
    email: [""]
  });
  
  console.log("✓ Angular ready!");
}
```

That's it! No bundling, no configuration, no boilerplate.

## What's Included

The built-in `art/angular` module provides:

- **@angular/core** - Components, decorators, dependency injection
- **@angular/common** - Built-in directives and pipes
- **@angular/forms** - FormControl, FormGroup, FormBuilder, validators
- **@angular/platform-browser** - BrowserModule, DomSanitizer
- **rxjs** - Observable, Subject, operators (map, filter, take)

## Using Angular Features

### Components and Decorators

```typescript
const Angular = require("@angular/core");

@Angular.Component({
  selector: "app-user",
  template: "<h1>{{name}}</h1>"
})
class UserComponent {
  name: string = "John";
}

// Lifecycle hooks are available
class MyComponent extends Angular.OnInit {
  ngOnInit() {
    console.log("Component initialized");
  }
}
```

### Reactive Forms

```typescript
const Forms = require("@angular/forms");

// Using FormBuilder
const form = new Forms.FormBuilder().group({
  username: ["john_doe"],
  email: ["john@example.com"],
  password: [""]
});

// Access controls
form.get("username").setValue("jane_doe");

// Validate
console.log("Form valid:", form.valid);
```

### Services and Dependency Injection

```typescript
const Angular = require("@angular/core");

@Angular.Injectable()
class DataService {
  getData(): string[] {
    return ["item1", "item2", "item3"];
  }
}

const service = getService(DataService);
console.log(service.getData());
```

### RxJS Observables

```typescript
const rxjs = require("rxjs");
const { map, filter } = rxjs.operators;

// Create observable
const source = rxjs.Observable.of(1, 2, 3, 4, 5);

// Transform and subscribe
source
  .pipe(
    filter((x: number) => x > 2),
    map((x: number) => x * 2)
  )
  .subscribe({
    next: (value: number) => console.log("Value:", value)
  });
```

## Advanced: Custom Angular Bundling

If you need a newer version of Angular or custom features not in the built-in module, you can bundle your own:

### 1. Install Dependencies

```bash
npm install @angular/core @angular/common @angular/forms \
            rxjs typescript
```

### 2. Create Bundle Entry

```javascript
// bundle-angular.js
export * from "@angular/core";
export * from "@angular/common";
export * from "@angular/forms";
```

### 3. Bundle with esbuild

```bash
npx esbuild bundle-angular.js --bundle --minify --format=iife --outfile=angular-bundle.js
```

### 4. Register Custom Module

```typescript
import { registerModule, require } from "art/modules";

// Register your custom bundle
registerModule("@angular/custom", function(module, exports, require) {
  // Embed bundled code here
  const angularCode = `/* bundled angular code */`;
  // ... setup exports
});
```

## Available Built-in Angular Modules

| Module | Contents |
|--------|----------|
| `@angular/core` | Components, decorators, lifecycle hooks, dependency injection |
| `@angular/common` | CommonModule, directives (NgIf, NgFor, NgClass), pipes (DatePipe, UpperCasePipe) |
| `@angular/forms` | FormControl, FormGroup, FormArray, FormBuilder, validators |
| `@angular/platform-browser` | BrowserModule, DomSanitizer, bootstrapApplication |
| `rxjs` | Observable, Subject, operators (map, filter, take) |

## API Reference

### setupAngular()
Initializes all built-in Angular and RxJS modules. Call this once at startup.

```typescript
import { setupAngular } from "art/angular";

setupAngular();
```

### Helper Functions

```typescript
// Create forms easily
const form = createFormGroup({
  name: ["John"],
  email: ["john@example.com"]
});

// Create form controls
const control = createFormControl("initial value");

// Get a service instance
const service = getService(MyService);
```

## Common Patterns

### Complete Example: User Management

```typescript
import { setupAngular, createFormGroup, getService } from "art/angular";

setupAngular();

const Angular = require("@angular/core");
const Forms = require("@angular/forms");

// Service
@Angular.Injectable()
class UserService {
  users: any[] = [];
  
  addUser(user: any): void {
    this.users.push(user);
  }
  
  getUsers(): any[] {
    return this.users;
  }
}

// Component
@Angular.Component({
  selector: "app-user-manager",
  template: `<div>User Manager</div>`
})
class UserManagerComponent {
  form: any;
  users: any[] = [];
  
  constructor(service: UserService) {
    this.form = createFormGroup({
      name: [""],
      email: [""]
    });
    this.users = service.getUsers();
  }
  
  addUser(): void {
    const values = this.form.getValues();
    console.log("Adding user:", values);
  }
}

// Use it
const service = getService(UserService);
const component = new UserManagerComponent(service);
component.addUser();
```

## When to Use Built-in vs Custom Bundling

### Use Built-in Angular When:
✅ Using standard Angular features
✅ Want zero setup time
✅ Need rapid prototyping
✅ Want smaller bundle sizes
✅ Don't need cutting-edge features

### Use Custom Bundling When:
📦 Need newer Angular version
📦 Using specialized packages (@angular/router, @angular/http)
📦 Customizing Angular source
📦 Optimizing for size/performance

## Performance Characteristics

**Built-in Module:**
- Pre-compiled and cached
- Instant initialization with `setupAngular()`
- Minimal memory overhead
- Ideal for most applications

**Custom Bundles:**
- More control over features
- Potential size optimization
- Additional setup required
- Best for specialized use cases

## Troubleshooting

### Components not rendering
Make sure `setupAngular()` is called before accessing Angular:
```typescript
import { setupAngular } from "art/angular";

// ✓ Correct
setupAngular();
const Angular = require("@angular/core");

// ✗ Wrong
const Angular = require("@angular/core");  // Not initialized yet
```

### Form validation not working
Use the built-in Validators class:
```typescript
const Forms = require("@angular/forms");

const control = new Forms.FormControl(
  "",
  Forms.Validators.required
);
```

### Services not injecting
Use `getService()` helper:
```typescript
import { getService } from "art/angular";

const myService = getService(MyService);
```

## See Also

- [Module System Guide](../../docs/MODULES.md) - How modules work
- [Bundling Guide](../../docs/BUNDLING.md) - Custom bundling for other packages
- [ART Guide](../../docs/ART_GUIDE.md) - ART language for high-performance code
