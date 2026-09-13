// Module System for Artisan QuickJS Runtime
// Provides CommonJS and ES6 module support for organizing code and loading npm packages

type ModuleFactory = (module: any, exports: any, require: any) => void;

interface ModuleMetadata {
  id: string;
  loaded: boolean;
  cached: boolean;
  exportCount: number;
}

// Internal module registry and cache
const _modules = new Map<string, { factory: ModuleFactory | object; isES6: boolean }>();
const _cache = new Map<string, any>();

/**
 * Register a CommonJS-style module with a factory function
 *
 * @param id - Module identifier (e.g., "auth", "database", "api")
 * @param factory - Function that receives (module, exports, require) and populates exports
 * @returns true if registered, false if already exists
 */
export function registerModule(id: string, factory: ModuleFactory): boolean {
  if (_modules.has(id)) {
    return false;
  }
  _modules.set(id, { factory, isES6: false });
  return true;
}

/**
 * Alias for registerModule
 */
export function defineModule(id: string, factory: ModuleFactory): boolean {
  return registerModule(id, factory);
}

/**
 * Register an ES6-style module with named exports
 *
 * @param id - Module identifier
 * @param exportsObject - Object containing named exports and optional default export
 * @returns true if registered, false if already exists
 */
export function defineESModule(id: string, exportsObject: any): boolean {
  if (_modules.has(id)) {
    return false;
  }
  _modules.set(id, { factory: exportsObject, isES6: true });
  return true;
}

/**
 * Load a module using CommonJS-style require()
 * Modules are cached after first load to avoid re-execution
 *
 * @param id - Module identifier to load
 * @returns Module exports object, or null if not found
 */
export function require(id: string): any {
  // Return cached module if already loaded
  if (_cache.has(id)) {
    return _cache.get(id);
  }

  const moduleRecord = _modules.get(id);
  if (!moduleRecord) {
    return null;
  }

  // Handle ES6 modules - return exports directly
  if (moduleRecord.isES6) {
    const exports = moduleRecord.factory as any;
    _cache.set(id, exports);
    return exports;
  }

  // Handle CommonJS modules - execute factory with module/exports/require
  const module = { id, exports: {} };
  const factory = moduleRecord.factory as ModuleFactory;

  // Create local require function for this module's dependencies
  const localRequire = (depId: string) => {
    return require(depId);
  };

  // Execute factory function to populate exports
  factory(module, module.exports, localRequire);

  // Cache the exports
  _cache.set(id, module.exports);
  return module.exports;
}

/**
 * Import a specific named export from a module (ES6-style)
 *
 * @param id - Module identifier
 * @param exportName - Name of export to import (empty string for default export)
 * @returns The exported value, or null if not found
 */
export function importModule(id: string, exportName: string): any {
  const module = require(id);
  if (!module) {
    return null;
  }

  if (!exportName || exportName === "default") {
    return module.default || module;
  }

  return module[exportName] || null;
}

/**
 * Import all exports from a module (ES6 import * as)
 *
 * @param id - Module identifier
 * @returns Object containing all module exports
 */
export function importAll(id: string): any {
  return require(id);
}

/**
 * Get count of registered (not necessarily loaded) modules
 *
 * @returns Number of registered modules
 */
export function getModuleCount(): number {
  return _modules.size;
}

/**
 * Get count of loaded/cached modules
 *
 * @returns Number of modules in cache
 */
export function getCacheSize(): number {
  return _cache.size;
}

/**
 * Get array of all registered module IDs
 *
 * @returns Array of module identifier strings
 */
export function getModuleIds(): string[] {
  return Array.from(_modules.keys());
}

/**
 * Check if a module has been loaded and cached
 *
 * @param id - Module identifier
 * @returns true if module is in cache
 */
export function isModuleLoaded(id: string): boolean {
  return _cache.has(id);
}

/**
 * Get metadata about a registered module
 *
 * @param id - Module identifier
 * @returns Metadata object with id, loaded, cached, exportCount properties
 */
export function getModuleMetadata(id: string): ModuleMetadata | null {
  const module = _modules.get(id);
  if (!module) {
    return null;
  }

  const loaded = _cache.has(id);
  const exports = loaded ? _cache.get(id) : {};
  const exportCount = Object.keys(exports).length;

  return {
    id,
    loaded,
    cached: loaded,
    exportCount
  };
}

/**
 * Clear all cached module instances (without unregistering them)
 * Modules will be re-executed on next require() call
 */
export function clearModuleCache(): void {
  _cache.clear();
}

/**
 * Clear all modules (both registry and cache)
 * Use with caution - removes all registered modules
 */
export function clearAllModules(): void {
  _modules.clear();
  _cache.clear();
}

/**
 * Parse an import statement to extract module ID and export name
 * Handles: "import X from 'module'"
 *          "import { named } from 'module'"
 *          "import * as all from 'module'"
 *
 * @param statement - Import statement string
 * @returns Object with moduleId and exportName properties
 */
export function parseImportStatement(statement: string): { moduleId: string; exportName: string } {
  const match = statement.match(/['"]([^'"]+)['"]/);
  const moduleId = match ? match[1] : "";

  // Extract export name from "import X from" pattern
  const nameMatch = statement.match(/import\s+(\w+)\s+from/);
  const exportName = nameMatch ? nameMatch[1] : "";

  return { moduleId, exportName };
}

/**
 * Resolve a module path (basic version - can be enhanced with search paths)
 *
 * @param path - Path to resolve
 * @returns Resolved path/ID
 */
export function resolveModulePath(path: string): string {
  // Remove .js/.ts extensions
  if (path.endsWith(".js") || path.endsWith(".ts")) {
    return path.substring(0, path.length - 3);
  }
  return path;
}

/**
 * Initialize the module system with built-in modules
 * Registers art/* modules that bridge to ART stdlib functionality
 */
export function initializeModuleSystem(): void {
  // Core modules are registered by the runtime/ART code
  // This function is a hook for future initialization logic
}

/**
 * Get statistics about module system usage
 *
 * @returns Object with modules, cached, and totalExports counts
 */
export function getModuleStatistics(): { modules: number; cached: number; totalExports: number } {
  let totalExports = 0;

  _cache.forEach((exports) => {
    totalExports += Object.keys(exports).length;
  });

  return {
    modules: _modules.size,
    cached: _cache.size,
    totalExports
  };
}

/**
 * Unregister a module (removes from registry and cache)
 *
 * @param id - Module identifier to unregister
 * @returns true if module was unregistered, false if not found
 */
export function unregisterModule(id: string): boolean {
  _cache.delete(id);
  return _modules.delete(id);
}

/**
 * Get all module exports for inspection/debugging
 *
 * @returns Map of module ID to exports object
 */
export function getAllModuleExports(): Map<string, any> {
  const result = new Map<string, any>();

  _modules.forEach((record, id) => {
    if (_cache.has(id)) {
      result.set(id, _cache.get(id));
    } else {
      // For uncached modules, show empty object
      result.set(id, {});
    }
  });

  return result;
}

/**
 * List all modules with their load status and export count
 *
 * @returns Array of module metadata
 */
export function listAllModules(): ModuleMetadata[] {
  return Array.from(_modules.keys()).map(id => {
    const metadata = getModuleMetadata(id);
    return metadata || { id, loaded: false, cached: false, exportCount: 0 };
  });
}
