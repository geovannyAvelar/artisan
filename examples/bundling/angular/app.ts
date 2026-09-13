// Example: Using Angular in Artisan - Simplified with Built-in Module
// Angular and RxJS are automatically initialized with setupAngular()
// No manual registration needed!

import { setupAngular, createComponent, createFormGroup, createFormControl, getService } from "art/angular";

export function setupApplication(): void {
  // One line to set up everything!
  setupAngular();

  // Initialize application
  initializeAngularApp();
}

// Angular modules are automatically registered by setupAngular()!

// ============================================================================
// Application Initialization
// ============================================================================

function initializeAngularApp(): void {
  console.log("✓ Angular initialized!");

  demonstrateComponents();
  demonstrateForms();
  demonstrateServices();
}

// Example 1: Components
function demonstrateComponents(): void {
  const Angular = require("@angular/core");

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

  const component = new HelloComponent();
  console.log("✓ Component:", component.greet());
}

// Example 2: Forms with FormBuilder
function demonstrateForms(): void {
  const form = createFormGroup({
    username: ["john_doe"],
    email: ["john@example.com"],
    password: [""]
  });

  console.log("✓ Form created:", form.getValues());
}

// Example 3: Services with Dependency Injection
function demonstrateServices(): void {
  const Angular = require("@angular/core");

  @Angular.Injectable()
  class UserService {
    getUser(): any {
      return { name: "John", email: "john@example.com" };
    }
  }

  const service = getService(UserService);
  console.log("✓ Service:", service.getUser());
}
