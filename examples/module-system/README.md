# Module System Example

This example demonstrates the **CommonJS and ES6 module system** in Artisan, showing how to organize applications into reusable modules with dependencies.

## What This Shows

- **Multi-module architecture** - Organizing code into logical modules
- **Module dependencies** - Modules requiring other modules
- **CommonJS pattern** - Using `require()` and `module.exports`
- **Configuration management** - Centralizing app config
- **Service-oriented design** - Database, API, Auth as services
- **Advanced patterns** - Factories, middleware, service locator

## Project Structure

```
module-system/
├── app.ts          - Main application with all modules
└── README.md       - This file
```

## Application Architecture

### Modules

1. **config** - Application configuration
   - API URLs
   - Environment settings
   - Timeouts and parameters

2. **database** - Data persistence
   - Connection management
   - Data storage and retrieval
   - Connection state tracking

3. **api** - HTTP API wrapper
   - Depends on: config
   - Endpoint construction
   - Request building

4. **auth** - Authentication service
   - Depends on: database
   - User login/logout
   - Authentication state

5. **app** - Main application
   - Depends on: config, database, api, auth
   - Application lifecycle
   - Coordination of services

### Dependency Graph

```
config ──────────┐
                 │
database ──┐     │
           │     │
      auth─┼─────┤
           │     │
        app ────┐│
               │││
           api──┘┘
               
Other dependencies:
- api depends on: config
- auth depends on: database
- app depends on: config, database, api, auth
```

## Running the Example

### Setup

```typescript
import { setupApplication } from "./app.ts";

// Initialize and start the application
setupApplication();
```

Output:
```
=== Application Starting ===
✓ Database connected
✓ Config loaded: https://api.example.com
✓ User authenticated
✓ Current user: user
✓ API endpoint: /api/v1/users
=== Application Ready ===
```

### Advanced Features Demo

```typescript
import { demonstrateAdvancedFeatures } from "./app.ts";

demonstrateAdvancedFeatures();
```

Demonstrates:
- **Factory Pattern** - Creating instances with auto-incrementing IDs
- **Middleware Pattern** - Chaining transformations
- **Service Locator** - Dynamic service registration and retrieval

## Key Concepts

### Module Registration

```typescript
registerModule("database", function(module, exports, require) {
  // Module implementation
  exports.connect = () => { /* ... */ };
  exports.save = (key, value) => { /* ... */ };
});
```

### Dependency Injection

```typescript
registerModule("api", function(module, exports, require) {
  let config = require("config");  // Get dependency
  
  exports.get = (path) => config.apiUrl + path;
});
```

### Module Loading

```typescript
let app = require("app");
app.start();  // Start the application
```

## Advanced Patterns Demonstrated

### 1. Factory Pattern
Creates new instances with automatic ID generation:
```typescript
let factory = require("userFactory");
let user1 = factory.createUser("Alice");  // {id: 1, name: "Alice"}
let user2 = factory.createUser("Bob");    // {id: 2, name: "Bob"}
```

### 2. Middleware Pattern
Chains transformations in a pipeline:
```typescript
let pipe = require("pipeline");
pipe.add(value => value + 1);      // Add 1
pipe.add(value => value * 2);      // Multiply by 2
pipe.add(value => value - 5);      // Subtract 5

pipe.execute(10);  // (10 + 1) * 2 - 5 = 17
```

### 3. Service Locator
Dynamically finds and registers services:
```typescript
let locator = require("serviceLocator");
locator.register("logger", loggerService);
let logger = locator.get("logger");  // Retrieve service
```

## Testing the Modules

Each module can be tested independently:

```typescript
// Test config
let config = require("config");
console.log(config.apiUrl);  // https://api.example.com

// Test database
let db = require("database");
db.connect();
db.save("key", "value");

// Test auth
let auth = require("auth");
auth.login("user", "password");
console.log(auth.isAuthenticated());  // true

// Test app
let app = require("app");
app.start();
app.stop();
```

## Real-World Application

This pattern is used in production applications for:

- **Microservices** - Each service is a module
- **Plugin systems** - Plugins register themselves as modules
- **Framework development** - Core framework + optional features
- **Multi-tenant apps** - Shared modules + tenant-specific modules
- **Large applications** - Break monolith into manageable pieces

## Benefits of This Structure

✅ **Modularity** - Each module has single responsibility
✅ **Reusability** - Modules can be used in different contexts
✅ **Testability** - Mock dependencies easily
✅ **Maintainability** - Changes localized to modules
✅ **Scalability** - Add features without modifying existing modules
✅ **Dependency injection** - Loose coupling via require()

## Extending This Example

### Add a New Module

```typescript
function registerEmailModule(): void {
  registerModule("email", function(module, exports, require) {
    let config = require("config");
    
    exports.send = function(to, subject, body) {
      // Send email implementation
      return true;
    };
  });
}

// Add to setupApplication():
registerEmailModule();
```

### Add New Service Dependencies

```typescript
registerModule("userService", function(module, exports, require) {
  let db = require("database");
  let email = require("email");
  let auth = require("auth");
  
  exports.registerUser = function(username, password, emailAddr) {
    // 1. Check auth
    // 2. Save to database
    // 3. Send welcome email
  };
});
```

### Add Middleware

```typescript
registerModule("logging", function(module, exports, require) {
  exports.logCall = function(moduleName, funcName, args) {
    console.log("[" + moduleName + "] " + funcName + " called");
  };
});
```

## Best Practices

1. **Keep modules focused** - One responsibility per module
2. **Minimize dependencies** - Reduce circular dependencies
3. **Export clean interfaces** - Only export what's needed
4. **Use consistent patterns** - Factory, singleton, middleware
5. **Document dependencies** - Make require() calls clear
6. **Test in isolation** - Mock dependencies in tests

## See Also

- [Modules Documentation](../../MODULES.md)
- [React Integration](../react-counter/)
- [Artisan README](../../README.md)
- [Stdlib Modules](../../art/stdlib/)

## Summary

This example shows how the module system enables:
- **Large applications** with multiple services
- **Code organization** with clear dependencies
- **Design patterns** (factory, middleware, service locator)
- **Maintainability** through modular architecture
- **Testability** with dependency injection

The same patterns work for Angular, Express, custom frameworks, and any multi-file JavaScript application.

---

**Status**: ✅ Ready to use

**Last Updated**: 2026-09-13
