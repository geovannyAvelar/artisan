// Example: Using Bundled Angular in Artisan
// This demonstrates how to register and use Angular from a bundle

import { registerModule, require } from "art/modules";

// Angular bundle code (would be embedded from bundles/angular-full.js)
// In a real scenario, you'd:
// 1. Bundle Angular with esbuild (see bundle.sh)
// 2. Generate module wrappers (see wrap-bundle.py)
// 3. Import and use here

export function setupApplication(): void {
  // Register RxJS module (dependency)
  registerRxJS();

  // Register Angular modules
  registerAngularModules();

  // Initialize application
  initializeAngularApp();
}

// ============================================================================
// Module Registration
// ============================================================================

function registerRxJS(): void {
  // In a real app, this would load the bundled RxJS code
  // For now, we demonstrate the pattern with a minimal RxJS-like export
  registerModule("rxjs", function(module, exports, require) {
    // The actual bundled RxJS code would go here
    // For demo, we'll export the RxJS API signatures

    // Observable class
    exports.Observable = class Observable {
      static of(...items: any[]): Observable {
        return new Observable((subscriber: any) => {
          items.forEach((item: any) => subscriber.next(item));
          subscriber.complete();
        });
      }

      subscribe(observer: any): any {
        return { unsubscribe: () => {} };
      }
    };

    // Operators
    exports.operators = {
      map: (fn: any) => (source: any) => ({
        subscribe: (observer: any) => source.subscribe({
          next: (value: any) => observer.next(fn(value)),
          error: (err: any) => observer.error(err),
          complete: () => observer.complete()
        })
      })
    };
  });
}

function registerAngularModules(): void {
  // Register Angular Core
  registerModule("@angular/core", function(module, exports, require) {
    // Get RxJS first (Angular depends on it)
    const rxjs = require("rxjs");

    // Component decorator
    exports.Component = function(config: any) {
      return function(constructor: any) {
        const instance = new constructor();
        instance.__config = config;
        return constructor;
      };
    };

    // NgModule decorator
    exports.NgModule = function(config: any) {
      return function(constructor: any) {
        const instance = new constructor();
        instance.__ngModule = config;
        return constructor;
      };
    };

    // Injectable decorator (for dependency injection)
    exports.Injectable = function(config?: any) {
      return function(constructor: any) {
        constructor.__injectable = true;
        return constructor;
      };
    };

    // Input/Output decorators
    exports.Input = function() {
      return function(target: any, propertyKey: string) {
        target.__inputs = target.__inputs || {};
        target.__inputs[propertyKey] = true;
      };
    };

    exports.Output = function() {
      return function(target: any, propertyKey: string) {
        target.__outputs = target.__outputs || {};
        target.__outputs[propertyKey] = true;
      };
    };

    // OnInit, OnDestroy lifecycle hooks
    exports.OnInit = class OnInit {
      ngOnInit() {}
    };

    exports.OnDestroy = class OnDestroy {
      ngOnDestroy() {}
    };

    // Injector for dependency resolution
    exports.Injector = class Injector {
      static create(config: any) {
        return {
          get(token: any) {
            // Simple DI implementation
            if (typeof token === "function") {
              return new token();
            }
            return null;
          }
        };
      }

      get(token: any) {
        return null;
      }
    };

    // Version info
    exports.VERSION = "15.0.0";
  });

  // Register Angular Common
  registerModule("@angular/common", function(module, exports, require) {
    const core = require("@angular/core");

    // Re-export everything from core
    Object.assign(exports, core);

    // Add common-specific exports
    exports.CommonModule = class CommonModule {};

    exports.NgIf = class NgIf {
      constructor(private condition: boolean) {}
    };

    exports.NgFor = class NgFor {
      constructor(private items: any[]) {}
    };

    exports.NgClass = class NgClass {
      constructor(private classes: any) {}
    };

    exports.DatePipe = class DatePipe {
      transform(value: any): string {
        return new Date(value).toISOString();
      }
    };

    exports.UpperCasePipe = class UpperCasePipe {
      transform(value: string): string {
        return value.toUpperCase();
      }
    };
  });

  // Register Angular Forms
  registerModule("@angular/forms", function(module, exports, require) {
    const core = require("@angular/core");

    Object.assign(exports, core);

    // FormControl
    exports.FormControl = class FormControl {
      value: any = null;

      setValue(value: any): void {
        this.value = value;
      }

      getValue(): any {
        return this.value;
      }
    };

    // FormGroup
    exports.FormGroup = class FormGroup {
      private controls: Map<string, any> = new Map();

      addControl(name: string, control: any): void {
        this.controls.set(name, control);
      }

      get(name: string): any {
        return this.controls.get(name);
      }

      getValues(): any {
        const values: any = {};
        this.controls.forEach((control, name) => {
          values[name] = control.value;
        });
        return values;
      }
    };

    // FormBuilder service
    exports.FormBuilder = class FormBuilder {
      group(controls: any): any {
        const group = new exports.FormGroup();
        Object.keys(controls).forEach(key => {
          const control = new exports.FormControl();
          if (Array.isArray(controls[key])) {
            control.setValue(controls[key][0]);
          }
          group.addControl(key, control);
        });
        return group;
      }

      control(value: any): any {
        const ctrl = new exports.FormControl();
        ctrl.setValue(value);
        return ctrl;
      }
    };

    exports.FormsModule = class FormsModule {};
    exports.ReactiveFormsModule = class ReactiveFormsModule {};
  });
}

// ============================================================================
// Application Initialization
// ============================================================================

function initializeAngularApp(): void {
  console.log("Initializing Angular application...");

  // Get modules
  const Angular = require("@angular/core");
  const CommonModule = require("@angular/common");
  const FormsModule = require("@angular/forms");

  console.log("✓ Angular loaded");
  console.log("✓ Angular version:", Angular.VERSION);
  console.log("✓ Component decorator available:", typeof Angular.Component);
  console.log("✓ NgModule decorator available:", typeof Angular.NgModule);

  // Example: Create a simple component
  createExampleComponent();

  // Example: Create a form component
  createFormComponent();

  // Example: Use services with dependency injection
  createServiceComponent();
}

function createExampleComponent(): void {
  const Angular = require("@angular/core");

  // Define a simple component
  @Angular.Component({
    selector: "app-hello",
    template: "<h1>Hello from Angular!</h1>"
  })
  class HelloComponent {
    name: string = "World";

    greet(): string {
      return "Hello, " + this.name;
    }
  }

  // Use it
  const instance = new HelloComponent();
  console.log("✓ Component created:", instance.greet());
}

function createFormComponent(): void {
  const FormsModule = require("@angular/forms");

  // Create a form using FormBuilder
  const formBuilder = new FormsModule.FormBuilder();

  const form = formBuilder.group({
    username: [""],
    email: [""],
    password: [""]
  });

  // Set values
  form.get("username").setValue("john_doe");
  form.get("email").setValue("john@example.com");

  // Get all values
  const values = form.getValues();
  console.log("✓ Form created with values:", values);
}

function createServiceComponent(): void {
  const Angular = require("@angular/core");

  // Define a service
  @Angular.Injectable()
  class DataService {
    getData(): string[] {
      return ["item1", "item2", "item3"];
    }
  }

  // Define a component that uses the service
  @Angular.Component({
    selector: "app-data",
    template: "<div>Data service loaded</div>"
  })
  class DataComponent {
    data: string[] = [];

    constructor(private service: DataService) {
      this.data = service.getData();
    }
  }

  // Use it
  const service = new DataService();
  const component = new DataComponent(service);
  console.log("✓ Service created, data:", component.data);
}

// ============================================================================
// Advanced: Module Communication
// ============================================================================

export function demonstrateModuleCommunication(): void {
  console.log("\n=== Module Communication Demo ===");

  const Angular = require("@angular/core");
  const Common = require("@angular/common");
  const Forms = require("@angular/forms");

  console.log("All modules loaded successfully:");
  console.log("  - @angular/core:", Object.keys(Angular).slice(0, 5).join(", ") + "...");
  console.log("  - @angular/common:", Object.keys(Common).slice(0, 5).join(", ") + "...");
  console.log("  - @angular/forms:", Object.keys(Forms).slice(0, 5).join(", ") + "...");
}

// ============================================================================
// Usage in main application
// ============================================================================

// Called from ART/main application
// Example integration:
/*

import { setupApplication, demonstrateModuleCommunication } from "./app.ts";

function onAppStart(): void {
  // Setup all Angular modules
  setupApplication();

  // Demonstrate communication
  demonstrateModuleCommunication();

  // Now use Angular in your UI
  // The module system handles loading and caching
}

*/
