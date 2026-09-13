// Module system for ART. Import with: `import { require, createModuleLoader, ... } from "art/modules";`
// Provides ES6 module and CommonJS support for JavaScript runtime.

// Module metadata type
export type ModuleMetadata = [id: string, exports: any, loaded: boolean, cached: boolean];

// Module factory type - function that loads a module
export type ModuleFactory = (module: any, exports: any, require: (id: string) => any) => void;

// Module registry - maps module IDs to their factories
type ModuleRegistry = [id: string, factory: ModuleFactory][];
let _modules: ModuleRegistry = [];

// Module cache - stores loaded modules
type ModuleCache = [id: string, exports: any][];
let _cache: ModuleCache = [];

// Current module context for tracking
type ModuleContext = [id: string, parent: string, loaded: boolean];
let _currentContext: ModuleContext = ["<main>", "", false];

// Module loader state
let _loaderInitialized: boolean = false;

// Initialize the module system
export function initializeModuleSystem(): boolean {
  _loaderInitialized = true;
  return true;
}

// Register a module with the system
export function registerModule(id: string, factory: ModuleFactory): boolean {
  // Check if already registered
  let i: number = 0;
  while (i < _modules.length) {
    if (_modules[i][0] == id) {
      return false; // Already registered
    }
    i = i + 1;
  }
  
  _modules = _modules + [[id, factory]];
  return true;
}

// Find a module factory by ID
function findModuleFactory(id: string): ModuleFactory | null {
  let i: number = 0;
  while (i < _modules.length) {
    if (_modules[i][0] == id) {
      return _modules[i][1];
    }
    i = i + 1;
  }
  return null;
}

// Check if module is in cache
function isCached(id: string): boolean {
  let i: number = 0;
  while (i < _cache.length) {
    if (_cache[i][0] == id) {
      return true;
    }
    i = i + 1;
  }
  return false;
}

// Get cached module exports
function getCachedExports(id: string): any {
  let i: number = 0;
  while (i < _cache.length) {
    if (_cache[i][0] == id) {
      return _cache[i][1];
    }
    i = i + 1;
  }
  return null;
}

// Cache module exports
function cacheModule(id: string, exports: any): void {
  _cache = _cache + [[id, exports]];
}

// The require() function - core of CommonJS module loading
export function require(id: string): any {
  // Return cached module if available
  if (isCached(id)) {
    return getCachedExports(id);
  }
  
  // Find the module factory
  let factory = findModuleFactory(id);
  if (factory == null) {
    return null; // Module not found
  }
  
  // Create module wrapper object (CommonJS pattern)
  let module: any = [id, null]; // [id, exports]
  let exports: any = {}; // Start with empty exports
  module[1] = exports;
  
  // Create bound require function for this module
  let boundRequire: (modId: string) => any = function(modId: string): any {
    return require(modId);
  };
  
  // Execute module factory
  factory(module, exports, boundRequire);
  
  // Cache the exports
  let finalExports: any = module[1];
  if (finalExports == null) {
    finalExports = exports;
  }
  cacheModule(id, finalExports);
  
  return finalExports;
}

// ES6 import equivalent - loads a module and returns specific export
export function importModule(id: string, exportName: string): any {
  let exports: any = require(id);
  if (exports == null) { return null; }
  
  // If exporting named export
  if (exportName.length > 0) {
    return exports[exportName];
  }
  
  // If importing default
  return exports["default"] != null ? exports["default"] : exports;
}

// ES6 import * as - loads all exports from module
export function importAll(id: string): any {
  return require(id);
}

// Define a module with CommonJS pattern
export function defineModule(id: string, factory: ModuleFactory): boolean {
  return registerModule(id, factory);
}

// Define module with ES6 style (returns object with named exports)
export function defineESModule(id: string, exportsObj: any): boolean {
  let factory: ModuleFactory = function(module: any, exports: any, require: (id: string) => any): void {
    // Copy all named exports
    let keys: string[] = getObjectKeys(exportsObj);
    let i: number = 0;
    while (i < keys.length) {
      let key: string = keys[i];
      exports[key] = exportsObj[key];
      i = i + 1;
    }
    
    // Also set as module.exports
    module[1] = exports;
  };
  
  return registerModule(id, factory);
}

// Get all module IDs
export function getModuleIds(): string[] {
  let result: string[] = [];
  let i: number = 0;
  while (i < _modules.length) {
    result = result + [_modules[i][0]];
    i = i + 1;
  }
  return result;
}

// Get count of registered modules
export function getModuleCount(): number {
  return _modules.length;
}

// Get count of cached modules
export function getCacheSize(): number {
  return _cache.length;
}

// Check if module is loaded and cached
export function isModuleLoaded(id: string): boolean {
  return isCached(id);
}

// Clear module cache
export function clearModuleCache(): void {
  _cache = [];
}

// Clear all modules and cache
export function clearAllModules(): void {
  _modules = [];
  _cache = [];
  _loaderInitialized = false;
}

// Get module metadata for inspection
export function getModuleMetadata(id: string): ModuleMetadata | null {
  let loaded: boolean = isCached(id);
  let cached: boolean = loaded;
  let exports: any = loaded ? getCachedExports(id) : null;
  
  return [id, exports, loaded, cached];
}

// Helper: Get object keys (used internally)
function getObjectKeys(obj: any): string[] {
  let result: string[] = [];
  // In ART, we'd need to iterate properties
  // This is a simplified version
  return result;
}

// Create a module bundle - combining multiple modules into one
export function createModuleBundle(moduleIds: string[]): string {
  let bundle: string = "";
  let i: number = 0;
  
  while (i < moduleIds.length) {
    let id: string = moduleIds[i];
    bundle = bundle + "// Module: " + id + "\n";
    
    let factory = findModuleFactory(id);
    if (factory != null) {
      bundle = bundle + "registerModule(\"" + id + "\", ...);\n";
    }
    
    i = i + 1;
  }
  
  return bundle;
}

// Resolve module path (normalize and validate)
export function resolveModulePath(importPath: string): string {
  // Handle built-in modules
  if (importPath == "art/react") { return "art/react"; }
  if (importPath == "art/net") { return "art/net"; }
  if (importPath == "art/fs") { return "art/fs"; }
  
  // Handle relative paths (simplified)
  if (importPath.substring(0, 2) == "./") {
    return importPath.substring(2);
  }
  if (importPath.substring(0, 3) == "../") {
    return importPath.substring(3);
  }
  
  // Handle node_modules style
  if (importPath.substring(0, 1) != ".") {
    return "node_modules/" + importPath;
  }
  
  return importPath;
}

// Parse import statement (basic parsing)
export function parseImportStatement(statement: string): [importType: string, source: string, names: string[]] {
  // Very simplified parsing - real implementation would be more robust
  
  // Detect ES6 import
  if (statement.substring(0, 6) == "import") {
    // Extract source (between quotes)
    let startQuote: number = indexOf(statement, "\"");
    if (startQuote < 0) {
      startQuote = indexOf(statement, "'");
    }
    if (startQuote >= 0) {
      let endQuote: number = indexOf(statement.substring(startQuote + 1), "\"");
      if (endQuote < 0) {
        endQuote = indexOf(statement.substring(startQuote + 1), "'");
      }
      if (endQuote >= 0) {
        let source: string = statement.substring(startQuote + 1, startQuote + 1 + endQuote);
        return ["es6", source, []];
      }
    }
  }
  
  // Detect CommonJS require
  if (indexOf(statement, "require(") >= 0) {
    let startParen: number = indexOf(statement, "(");
    let endParen: number = indexOf(statement, ")");
    if (startParen >= 0 && endParen > startParen) {
      let args: string = statement.substring(startParen + 1, endParen);
      let startQuote: number = indexOf(args, "\"");
      if (startQuote < 0) {
        startQuote = indexOf(args, "'");
      }
      if (startQuote >= 0) {
        let endQuote: number = indexOf(args.substring(startQuote + 1), "\"");
        if (endQuote < 0) {
          endQuote = indexOf(args.substring(startQuote + 1), "'");
        }
        if (endQuote >= 0) {
          let source: string = args.substring(startQuote + 1, startQuote + 1 + endQuote);
          return ["commonjs", source, []];
        }
      }
    }
  }
  
  return ["unknown", "", []];
}

// Helper: Find index of substring
function indexOf(str: string, search: string): number {
  let i: number = 0;
  while (i <= str.length - search.length) {
    let match: boolean = true;
    let j: number = 0;
    while (j < search.length) {
      if (str.substring(i + j, i + j + 1) != search.substring(j, j + 1)) {
        match = false;
      }
      j = j + 1;
    }
    if (match) { return i; }
    i = i + 1;
  }
  return -1;
}
