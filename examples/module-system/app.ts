// Example: Using the Module System in Artisan
// This demonstrates a real-world multi-module application structure

import { registerModule, require, initializeModuleSystem } from "art/modules";

// Initialize the module system
export function setupApplication(): void {
  initializeModuleSystem();
  
  // Register core modules
  registerConfigModule();
  registerDatabaseModule();
  registerAPIModule();
  registerAuthModule();
  registerAppModule();
  
  // Boot the application
  let app = require("app");
  app.start();
}

// ============================================================================
// Core Application Modules
// ============================================================================

function registerConfigModule(): void {
  registerModule("config", function(module, exports, require) {
    exports.apiUrl = "https://api.example.com";
    exports.timeout = 5000;
    exports.environment = "production";
    exports.debug = false;
    
    exports.getConfig = function() {
      return {
        apiUrl: exports.apiUrl,
        timeout: exports.timeout,
        env: exports.environment
      };
    };
  });
}

function registerDatabaseModule(): void {
  registerModule("database", function(module, exports, require) {
    let isConnected = false;
    let data: any = {};
    
    exports.connect = function(): boolean {
      if (isConnected) { return false; }
      isConnected = true;
      return true;
    };
    
    exports.disconnect = function(): void {
      isConnected = false;
    };
    
    exports.save = function(key: string, value: any): boolean {
      if (!isConnected) { return false; }
      data[key] = value;
      return true;
    };
    
    exports.load = function(key: string): any {
      if (!isConnected) { return null; }
      return data[key];
    };
    
    exports.isConnected = function(): boolean {
      return isConnected;
    };
  });
}

function registerAPIModule(): void {
  registerModule("api", function(module, exports, require) {
    let config = require("config");
    
    exports.get = function(path: string): string {
      return config.apiUrl + path;
    };
    
    exports.post = function(path: string, data: any): string {
      return config.apiUrl + path + "?data=" + data.toString();
    };
    
    exports.endpoint = function(resource: string): string {
      return "/api/v1/" + resource;
    };
  });
}

function registerAuthModule(): void {
  registerModule("auth", function(module, exports, require) {
    let db = require("database");
    let currentUser: any = null;
    
    exports.login = function(username: string, password: string): boolean {
      // Simulate login
      if (username.length > 0 && password.length > 0) {
        currentUser = { username: username, loggedIn: true };
        db.save("currentUser", currentUser);
        return true;
      }
      return false;
    };
    
    exports.logout = function(): void {
      currentUser = null;
      db.save("currentUser", null);
    };
    
    exports.getCurrentUser = function(): any {
      return currentUser;
    };
    
    exports.isAuthenticated = function(): boolean {
      return currentUser != null;
    };
  });
}

function registerAppModule(): void {
  registerModule("app", function(module, exports, require) {
    let config = require("config");
    let db = require("database");
    let auth = require("auth");
    let api = require("api");
    
    exports.start = function(): void {
      console.log("=== Application Starting ===");
      
      // Connect to database
      if (db.connect()) {
        console.log("✓ Database connected");
      }
      
      // Load configuration
      let appConfig = config.getConfig();
      console.log("✓ Config loaded: " + appConfig.apiUrl);
      
      // Simulate user login
      if (auth.login("user", "password")) {
        console.log("✓ User authenticated");
        
        let user = auth.getCurrentUser();
        console.log("✓ Current user: " + user.username);
      }
      
      // Test API calls
      let endpoint = api.endpoint("users");
      console.log("✓ API endpoint: " + endpoint);
      
      // Save data
      db.save("appState", { initialized: true, startTime: 1234567890 });
      
      console.log("=== Application Ready ===");
    };
    
    exports.stop = function(): void {
      console.log("Stopping application...");
      auth.logout();
      db.disconnect();
      console.log("Application stopped");
    };
  });
}

// ============================================================================
// Example: Advanced Module Features
// ============================================================================

export function demonstrateAdvancedFeatures(): void {
  initializeModuleSystem();
  
  // Example 1: Factory Pattern
  registerModule("userFactory", function(module, exports, require) {
    let userCount = 0;
    
    exports.createUser = function(name: string): any {
      userCount = userCount + 1;
      return {
        id: userCount,
        name: name,
        active: true
      };
    };
  });
  
  let factory = require("userFactory");
  let user1 = factory.createUser("Alice");
  let user2 = factory.createUser("Bob");
  
  console.log("Created users: " + user1.id + ", " + user2.id);
  
  // Example 2: Middleware Pattern
  registerModule("pipeline", function(module, exports, require) {
    let stages: any[] = [];
    
    exports.add = function(fn: (value: any) => any): void {
      stages = stages + [fn];
    };
    
    exports.execute = function(value: any): any {
      let i = 0;
      let result = value;
      while (i < stages.length) {
        result = stages[i](result);
        i = i + 1;
      }
      return result;
    };
  });
  
  let pipe = require("pipeline");
  pipe.add(function(v: number): number { return v + 1; });
  pipe.add(function(v: number): number { return v * 2; });
  pipe.add(function(v: number): number { return v - 5; });
  
  let pipeResult = pipe.execute(10);
  console.log("Pipeline result (10 -> +1 -> *2 -> -5): " + pipeResult);
  
  // Example 3: Service Locator Pattern
  registerModule("serviceLocator", function(module, exports, require) {
    let services: any = {};
    
    exports.register = function(name: string, service: any): void {
      services[name] = service;
    };
    
    exports.get = function(name: string): any {
      return services[name];
    };
  });
  
  let locator = require("serviceLocator");
  locator.register("logger", { log: (msg: string) => console.log(msg) });
  locator.register("database", { query: () => "data" });
  
  let logger = locator.get("logger");
  logger.log("Service located!");
}

// ============================================================================
// Entry point
// ============================================================================

// Uncomment to run:
// setupApplication();
// demonstrateAdvancedFeatures();
