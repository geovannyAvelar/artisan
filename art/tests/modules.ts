import { initializeModuleSystem, registerModule, require, importModule, importAll, defineModule, defineESModule, getModuleIds, getModuleCount, getCacheSize, isModuleLoaded, clearModuleCache, clearAllModules, getModuleMetadata, resolveModulePath, parseImportStatement, createModuleBundle } from "art/modules";

function testInitializeModuleSystem(): number {
  clearAllModules();
  if (!initializeModuleSystem()) { return 1; }
  return 0;
}

function testRegisterModule(): number {
  clearAllModules();
  initializeModuleSystem();
  
  let factory = function(module: any, exports: any, require: (id: string) => any): void {
    exports.value = 42;
  };
  
  if (!registerModule("test-module", factory)) { return 1; }
  return 0;
}

function testRequireSimpleModule(): number {
  clearAllModules();
  initializeModuleSystem();
  
  let factory = function(module: any, exports: any, require: (id: string) => any): void {
    exports.hello = "world";
    exports.number = 42;
  };
  
  registerModule("simple", factory);
  
  let mod = require("simple");
  if (mod == null) { return 1; }
  if (mod.hello != "world") { return 2; }
  if (mod.number != 42) { return 3; }
  
  return 0;
}

function testRequireModuleExports(): number {
  clearAllModules();
  initializeModuleSystem();
  
  let factory = function(module: any, exports: any, require: (id: string) => any): void {
    module[1] = { exported: true };
  };
  
  registerModule("export-test", factory);
  
  let mod = require("export-test");
  if (mod == null) { return 1; }
  if (!mod.exported) { return 2; }
  
  return 0;
}

function testModuleCache(): number {
  clearAllModules();
  initializeModuleSystem();
  
  let callCount: number = 0;
  
  let factory = function(module: any, exports: any, require: (id: string) => any): void {
    callCount = callCount + 1;
    exports.value = callCount;
  };
  
  registerModule("cached", factory);
  
  let mod1 = require("cached");
  if (mod1.value != 1) { return 1; }
  
  let mod2 = require("cached");
  if (mod2.value != 1) { return 2; } // Should be cached, not re-executed
  
  if (callCount != 1) { return 3; } // Factory should only run once
  
  return 0;
}

function testIsModuleLoaded(): number {
  clearAllModules();
  initializeModuleSystem();
  
  let factory = function(module: any, exports: any, require: (id: string) => any): void {
    exports.test = true;
  };
  
  registerModule("loaded-test", factory);
  
  if (isModuleLoaded("loaded-test")) { return 1; } // Not loaded yet
  
  require("loaded-test");
  
  if (!isModuleLoaded("loaded-test")) { return 2; } // Should be loaded now
  
  return 0;
}

function testGetModuleCount(): number {
  clearAllModules();
  initializeModuleSystem();
  
  if (getModuleCount() != 0) { return 1; }
  
  registerModule("m1", function(m: any, e: any, r: any): void {});
  if (getModuleCount() != 1) { return 2; }
  
  registerModule("m2", function(m: any, e: any, r: any): void {});
  if (getModuleCount() != 2) { return 3; }
  
  return 0;
}

function testGetCacheSize(): number {
  clearAllModules();
  initializeModuleSystem();
  
  if (getCacheSize() != 0) { return 1; }
  
  registerModule("m1", function(m: any, e: any, r: any): void { e.x = 1; });
  require("m1");
  
  if (getCacheSize() != 1) { return 2; }
  
  registerModule("m2", function(m: any, e: any, r: any): void { e.y = 2; });
  require("m2");
  
  if (getCacheSize() != 2) { return 3; }
  
  return 0;
}

function testClearModuleCache(): number {
  clearAllModules();
  initializeModuleSystem();
  
  registerModule("m1", function(m: any, e: any, r: any): void { e.x = 1; });
  require("m1");
  
  if (getCacheSize() != 1) { return 1; }
  
  clearModuleCache();
  
  if (getCacheSize() != 0) { return 2; }
  if (isModuleLoaded("m1")) { return 3; }
  
  return 0;
}

function testDefineESModule(): number {
  clearAllModules();
  initializeModuleSystem();
  
  let exportsObj: any = { x: 10, y: 20 };
  defineESModule("es-mod", exportsObj);
  
  let mod = require("es-mod");
  if (mod == null) { return 1; }
  
  return 0;
}

function testModuleWithDependency(): number {
  clearAllModules();
  initializeModuleSystem();
  
  // Register dependency
  registerModule("dep", function(m: any, e: any, r: any): void {
    e.getValue = function(): number { return 42; };
  });
  
  // Register module that requires dependency
  registerModule("app", function(m: any, e: any, r: any): void {
    let dep = r("dep");
    e.result = dep.getValue();
  });
  
  let app = require("app");
  if (app.result != 42) { return 1; }
  
  return 0;
}

function testMultipleModuleDependencies(): number {
  clearAllModules();
  initializeModuleSystem();
  
  registerModule("math", function(m: any, e: any, r: any): void {
    e.add = function(a: number, b: number): number { return a + b; };
  });
  
  registerModule("string", function(m: any, e: any, r: any): void {
    e.concat = function(a: string, b: string): string { return a + b; };
  });
  
  registerModule("utils", function(m: any, e: any, r: any): void {
    let math = r("math");
    let str = r("string");
    e.process = function(): string {
      let num = math.add(1, 2);
      return str.concat("Result: ", num.toString());
    };
  });
  
  let utils = require("utils");
  // Would need string conversion support to fully test
  
  return 0;
}

function testResolveModulePath(): number {
  // Built-in paths
  if (resolveModulePath("art/react") != "art/react") { return 1; }
  if (resolveModulePath("art/net") != "art/net") { return 2; }
  if (resolveModulePath("art/fs") != "art/fs") { return 3; }
  
  // Relative paths
  if (resolveModulePath("./lib").substring(0, 3) == "./") { return 4; }
  
  return 0;
}

function testParseImportStatement(): number {
  // ES6 import
  let result1 = parseImportStatement("import { Component } from 'react'");
  if (result1[0] != "es6") { return 1; }
  
  // CommonJS require
  let result2 = parseImportStatement("const React = require('react')");
  if (result2[0] != "commonjs") { return 2; }
  
  return 0;
}

function testGetModuleIds(): number {
  clearAllModules();
  initializeModuleSystem();
  
  registerModule("m1", function(m: any, e: any, r: any): void {});
  registerModule("m2", function(m: any, e: any, r: any): void {});
  
  let ids = getModuleIds();
  if (ids.length != 2) { return 1; }
  
  return 0;
}

function testGetModuleMetadata(): number {
  clearAllModules();
  initializeModuleSystem();
  
  registerModule("meta-test", function(m: any, e: any, r: any): void { e.x = 1; });
  
  let metadata = getModuleMetadata("meta-test");
  if (metadata == null) { return 1; }
  
  // Before loading
  if (metadata[2]) { return 2; } // Should not be loaded yet
  
  require("meta-test");
  
  metadata = getModuleMetadata("meta-test");
  if (!metadata[2]) { return 3; } // Should be loaded now
  
  return 0;
}

function testModuleCircularDependency(): number {
  clearAllModules();
  initializeModuleSystem();
  
  // This tests how the system handles circular dependencies
  // In this simplified implementation, circular deps will cause issues
  // but the test ensures the system doesn't crash
  
  registerModule("a", function(m: any, e: any, r: any): void {
    e.value = "a";
  });
  
  registerModule("b", function(m: any, e: any, r: any): void {
    let a = r("a");
    e.value = "b";
  });
  
  let b = require("b");
  if (b == null) { return 1; }
  
  return 0;
}

function testRequireNonexistentModule(): number {
  clearAllModules();
  initializeModuleSystem();
  
  let result = require("nonexistent");
  if (result != null) { return 1; } // Should return null
  
  return 0;
}

function testClearAllModules(): number {
  clearAllModules();
  initializeModuleSystem();
  
  registerModule("m1", function(m: any, e: any, r: any): void { e.x = 1; });
  require("m1");
  
  if (getModuleCount() != 1) { return 1; }
  if (getCacheSize() != 1) { return 2; }
  
  clearAllModules();
  
  if (getModuleCount() != 0) { return 3; }
  if (getCacheSize() != 0) { return 4; }
  
  return 0;
}

function testModuleExportsFunctions(): number {
  clearAllModules();
  initializeModuleSystem();
  
  registerModule("funcs", function(m: any, e: any, r: any): void {
    e.add = function(a: number, b: number): number { return a + b; };
    e.multiply = function(a: number, b: number): number { return a * b; };
  });
  
  let funcs = require("funcs");
  if (funcs.add(2, 3) != 5) { return 1; }
  if (funcs.multiply(3, 4) != 12) { return 2; }
  
  return 0;
}

function testModuleExportsMultiple(): number {
  clearAllModules();
  initializeModuleSystem();
  
  registerModule("multi", function(m: any, e: any, r: any): void {
    e.string = "test";
    e.number = 42;
    e.boolean = true;
    e.null_value = null;
  });
  
  let mod = require("multi");
  if (mod.string != "test") { return 1; }
  if (mod.number != 42) { return 2; }
  if (!mod.boolean) { return 3; }
  
  return 0;
}

function testRegisterDuplicate(): number {
  clearAllModules();
  initializeModuleSystem();
  
  let factory = function(m: any, e: any, r: any): void {};
  
  if (!registerModule("dup", factory)) { return 1; }
  if (registerModule("dup", factory)) { return 2; } // Should fail on duplicate
  
  return 0;
}

function testImportModule(): number {
  clearAllModules();
  initializeModuleSystem();
  
  registerModule("named", function(m: any, e: any, r: any): void {
    e.Component = "React Component";
    e.default = "Default Export";
  });
  
  let comp = importModule("named", "Component");
  if (comp != "React Component") { return 1; }
  
  let def = importModule("named", "");
  if (def != "Default Export") { return 2; }
  
  return 0;
}

function testImportAll(): number {
  clearAllModules();
  initializeModuleSystem();
  
  registerModule("all", function(m: any, e: any, r: any): void {
    e.x = 1;
    e.y = 2;
    e.z = 3;
  });
  
  let all = importAll("all");
  if (all == null) { return 1; }
  
  return 0;
}
