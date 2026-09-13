// Angular integration module for ART. Import with: `import { setupAngular } from "art/angular";`
// Provides simplified Angular and RxJS setup with automatic module registration.

import { registerModule, require } from "art/modules";

// Initialize Angular with all core modules and RxJS
// Call this once at application startup
export function setupAngular(): void {
  registerRxJS();
  registerAngularCore();
  registerAngularCommon();
  registerAngularForms();
  registerAngularPlatformBrowser();
}

// ============================================================================
// RxJS Module Registration
// ============================================================================

function registerRxJS(): void {
  registerModule("rxjs", function(module, exports, require) {
    // Observable class for reactive programming
    exports.Observable = class Observable {
      static of(...items: any[]): Observable {
        return new Observable((subscriber: any) => {
          items.forEach((item: any) => subscriber.next(item));
          subscriber.complete();
        });
      }

      static from(iterable: any): Observable {
        return new Observable((subscriber: any) => {
          if (Array.isArray(iterable)) {
            iterable.forEach((item: any) => subscriber.next(item));
          }
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
      }),

      filter: (predicate: any) => (source: any) => ({
        subscribe: (observer: any) => source.subscribe({
          next: (value: any) => {
            if (predicate(value)) observer.next(value);
          },
          error: (err: any) => observer.error(err),
          complete: () => observer.complete()
        })
      }),

      take: (count: number) => (source: any) => ({
        subscribe: (observer: any) => {
          let taken: number = 0;
          return source.subscribe({
            next: (value: any) => {
              if (taken < count) {
                observer.next(value);
                taken = taken + 1;
                if (taken == count) observer.complete();
              }
            },
            error: (err: any) => observer.error(err),
            complete: () => observer.complete()
          });
        }
      })
    };

    // Subject for pub/sub
    exports.Subject = class Subject extends exports.Observable {
      private observers: any[] = [];

      next(value: any): void {
        this.observers.forEach((obs: any) => obs.next(value));
      }

      error(err: any): void {
        this.observers.forEach((obs: any) => obs.error(err));
      }

      complete(): void {
        this.observers.forEach((obs: any) => obs.complete());
      }

      subscribe(observer: any): any {
        this.observers.push(observer);
        return {
          unsubscribe: () => {
            this.observers = this.observers.filter((o: any) => o != observer);
          }
        };
      }
    };
  });
}

// ============================================================================
// Angular Core Module
// ============================================================================

function registerAngularCore(): void {
  registerModule("@angular/core", function(module, exports, require) {
    const rxjs = require("rxjs");

    // Component decorator
    exports.Component = function(config: any) {
      return function(constructor: any) {
        constructor.__component = config;
        return constructor;
      };
    };

    // Directive decorator
    exports.Directive = function(config: any) {
      return function(constructor: any) {
        constructor.__directive = config;
        return constructor;
      };
    };

    // NgModule decorator for module definitions
    exports.NgModule = function(config: any) {
      return function(constructor: any) {
        constructor.__ngModule = config;
        return constructor;
      };
    };

    // Injectable decorator for dependency injection
    exports.Injectable = function(config?: any) {
      return function(constructor: any) {
        constructor.__injectable = true;
        constructor.__injectableConfig = config;
        return constructor;
      };
    };

    // Input decorator for component properties
    exports.Input = function() {
      return function(target: any, propertyKey: string) {
        target.__inputs = target.__inputs || {};
        target.__inputs[propertyKey] = true;
      };
    };

    // Output decorator for component events
    exports.Output = function() {
      return function(target: any, propertyKey: string) {
        target.__outputs = target.__outputs || {};
        target.__outputs[propertyKey] = true;
      };
    };

    // ViewChild decorator
    exports.ViewChild = function(selector: any) {
      return function(target: any, propertyKey: string) {
        target.__viewChildren = target.__viewChildren || {};
        target.__viewChildren[propertyKey] = selector;
      };
    };

    // Lifecycle hooks
    exports.OnInit = class OnInit {
      ngOnInit() {}
    };

    exports.OnDestroy = class OnDestroy {
      ngOnDestroy() {}
    };

    exports.OnChanges = class OnChanges {
      ngOnChanges(changes: any) {}
    };

    exports.AfterViewInit = class AfterViewInit {
      ngAfterViewInit() {}
    };

    exports.AfterContentInit = class AfterContentInit {
      ngAfterContentInit() {}
    };

    // Dependency injection
    exports.Injector = class Injector {
      static create(config: any): Injector {
        const injector = new Injector();
        injector.__providers = config.providers || [];
        return injector;
      }

      get(token: any): any {
        if (typeof token === "function") {
          return new token();
        }
        return null;
      }
    };

    // Platform reference
    exports.PLATFORM_ID = "browser";

    // Version
    exports.VERSION = "15.0.0";
  });
}

// ============================================================================
// Angular Common Module
// ============================================================================

function registerAngularCommon(): void {
  registerModule("@angular/common", function(module, exports, require) {
    const core = require("@angular/core");

    // Re-export core
    Object.assign(exports, core);

    // CommonModule
    exports.CommonModule = class CommonModule {};

    // Built-in directives
    exports.NgIf = class NgIf {
      constructor(private condition: boolean) {}
    };

    exports.NgFor = class NgFor {
      constructor(private items: any[]) {}
    };

    exports.NgClass = class NgClass {
      constructor(private classes: any) {}
    };

    exports.NgStyle = class NgStyle {
      constructor(private styles: any) {}
    };

    exports.NgSwitch = class NgSwitch {
      constructor(private expr: any) {}
    };

    // Built-in pipes
    exports.DatePipe = class DatePipe {
      transform(value: any, format?: string): string {
        if (!value) return "";
        const date = new Date(value);
        return date.toISOString();
      }
    };

    exports.UpperCasePipe = class UpperCasePipe {
      transform(value: string): string {
        return value ? value.toUpperCase() : "";
      }
    };

    exports.LowerCasePipe = class LowerCasePipe {
      transform(value: string): string {
        return value ? value.toLowerCase() : "";
      }
    };

    exports.CurrencyPipe = class CurrencyPipe {
      transform(value: number, currency: string = "USD"): string {
        return currency + " " + value;
      }
    };

    exports.PercentPipe = class PercentPipe {
      transform(value: number): string {
        return value * 100 + "%";
      }
    };

    // Location service
    exports.Location = class Location {
      back(): void {}
      forward(): void {}
      go(path: string): void {}
    };
  });
}

// ============================================================================
// Angular Forms Module
// ============================================================================

function registerAngularForms(): void {
  registerModule("@angular/forms", function(module, exports, require) {
    const core = require("@angular/core");

    Object.assign(exports, core);

    // FormControl - manages a single form control value
    exports.FormControl = class FormControl {
      value: any = null;
      valid: boolean = true;
      errors: any = null;

      constructor(value?: any, validators?: any) {
        if (value !== undefined) {
          this.value = value;
        }
      }

      setValue(value: any): void {
        this.value = value;
      }

      getValue(): any {
        return this.value;
      }

      setErrors(errors: any): void {
        this.errors = errors;
        this.valid = !errors;
      }

      reset(): void {
        this.value = null;
      }
    };

    // FormGroup - manages a group of form controls
    exports.FormGroup = class FormGroup {
      private controls: Map<string, any> = new Map();
      valid: boolean = true;
      value: any = {};

      constructor(controls: any = {}) {
        Object.keys(controls).forEach((key: string) => {
          this.controls.set(key, controls[key]);
        });
        this.updateValue();
      }

      addControl(name: string, control: any): void {
        this.controls.set(name, control);
        this.updateValue();
      }

      removeControl(name: string): void {
        this.controls.delete(name);
        this.updateValue();
      }

      get(name: string): any {
        return this.controls.get(name);
      }

      getValues(): any {
        return this.value;
      }

      private updateValue(): void {
        const values: any = {};
        this.controls.forEach((control: any, name: string) => {
          values[name] = control.value;
        });
        this.value = values;
        this.valid = Array.from(this.controls.values()).every((c: any) => c.valid !== false);
      }

      reset(): void {
        this.controls.forEach((control: any) => control.reset());
        this.updateValue();
      }

      patchValue(values: any): void {
        Object.keys(values).forEach((key: string) => {
          const control = this.get(key);
          if (control) {
            control.setValue(values[key]);
          }
        });
        this.updateValue();
      }
    };

    // FormArray - manages an array of controls
    exports.FormArray = class FormArray {
      private controls: any[] = [];
      valid: boolean = true;
      value: any[] = [];

      constructor(controls: any[] = []) {
        this.controls = controls;
        this.updateValue();
      }

      push(control: any): void {
        this.controls.push(control);
        this.updateValue();
      }

      removeAt(index: number): void {
        this.controls.splice(index, 1);
        this.updateValue();
      }

      at(index: number): any {
        return this.controls[index];
      }

      private updateValue(): void {
        this.value = this.controls.map((c: any) => c.value);
        this.valid = this.controls.every((c: any) => c.valid !== false);
      }
    };

    // FormBuilder - helper for creating form structures
    exports.FormBuilder = class FormBuilder {
      group(controls: any): any {
        const formControls: any = {};
        Object.keys(controls).forEach((key: string) => {
          const config = controls[key];
          if (Array.isArray(config)) {
            formControls[key] = new exports.FormControl(config[0]);
          } else {
            formControls[key] = new exports.FormControl(config);
          }
        });
        return new exports.FormGroup(formControls);
      }

      control(value: any): any {
        return new exports.FormControl(value);
      }

      array(controls: any[]): any {
        return new exports.FormArray(controls);
      }
    };

    // Built-in validators
    exports.Validators = class Validators {
      static required(control: any): any {
        return control.value ? null : { required: true };
      }

      static minLength(length: number) {
        return function(control: any): any {
          return control.value && control.value.length < length
            ? { minLength: { length } }
            : null;
        };
      }

      static maxLength(length: number) {
        return function(control: any): any {
          return control.value && control.value.length > length
            ? { maxLength: { length } }
            : null;
        };
      }

      static email(control: any): any {
        const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
        return control.value && !emailRegex.test(control.value)
          ? { email: true }
          : null;
      }
    };

    // Module exports
    exports.FormsModule = class FormsModule {};
    exports.ReactiveFormsModule = class ReactiveFormsModule {};
  });
}

// ============================================================================
// Angular Platform Browser Module
// ============================================================================

function registerAngularPlatformBrowser(): void {
  registerModule("@angular/platform-browser", function(module, exports, require) {
    const core = require("@angular/core");

    Object.assign(exports, core);

    // BrowserModule
    exports.BrowserModule = class BrowserModule {};

    // DOM sanitization
    exports.DomSanitizer = class DomSanitizer {
      sanitize(context: any, value: string): string {
        return value;
      }

      bypassSecurityTrustHtml(value: string): any {
        return { __html: value };
      }

      bypassSecurityTrustStyle(value: string): any {
        return { __style: value };
      }

      bypassSecurityTrustResourceUrl(value: string): any {
        return { __url: value };
      }
    };

    // Bootstrap utilities
    exports.bootstrapApplication = function(
      rootComponent: any,
      config?: any
    ): any {
      console.log("Application bootstrapped with component:", rootComponent.name);
      return {};
    };
  });
}

// ============================================================================
// Helper Functions
// ============================================================================

// Get a service from the dependency injection container
export function getService(serviceClass: any): any {
  return new serviceClass();
}

// Create a simple Angular component with automatic setup
export function createComponent(selector: string, template: string, componentClass: any): any {
  const Angular = require("@angular/core");

  const config = {
    selector: selector,
    template: template
  };

  return Angular.Component(config)(componentClass);
}

// Shortcut to create a form group
export function createFormGroup(controls: any): any {
  const Forms = require("@angular/forms");
  return new Forms.FormGroup(controls);
}

// Shortcut to create a form control
export function createFormControl(value?: any): any {
  const Forms = require("@angular/forms");
  return new Forms.FormControl(value);
}
