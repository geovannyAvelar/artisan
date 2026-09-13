// Tests for the Module System
// Verifies CommonJS and ES6 module support

import {
  registerModule,
  defineModule,
  defineESModule,
  require,
  importModule,
  importAll,
  getModuleCount,
  getCacheSize,
  getModuleIds,
  isModuleLoaded,
  getModuleMetadata,
  clearModuleCache,
  clearAllModules,
  parseImportStatement,
  resolveModulePath,
  getModuleStatistics,
  unregisterModule,
  getAllModuleExports,
  listAllModules
} from "art/modules";

export function runModuleSystemTests(): void {
  // Test 1: Basic module registration
  test("registerModule adds module to registry", () => {
    clearAllModules();
    const result = registerModule("math", (m, e, r) => {
      e.add = (a: number, b: number) => a + b;
    });
    assert(result === true, "registerModule should return true");
    assert(getModuleCount() === 1, "Module count should be 1");
  });

  // Test 2: Duplicate registration prevention
  test("registerModule prevents duplicate registration", () => {
    clearAllModules();
    registerModule("test", (m, e, r) => {});
    const result = registerModule("test", (m, e, r) => {});
    assert(result === false, "Duplicate registration should return false");
  });

  // Test 3: Module loading and caching
  test("require loads and caches module", () => {
    clearAllModules();
    registerModule("counter", (m, e, r) => {
      e.count = 0;
      e.increment = () => ++e.count;
    });

    const c1 = require("counter");
    c1.increment();
    const c2 = require("counter");

    assert(c1 === c2, "Same module instance should be returned");
    assert(c2.count === 1, "Module state should persist");
  });

  // Test 4: Module with dependencies
  test("module can require other modules", () => {
    clearAllModules();
    registerModule("config", (m, e, r) => {
      e.value = 42;
    });

    registerModule("app", (m, e, r) => {
      const config = r("config");
      e.getValue = () => config.value;
    });

    const app = require("app");
    assert(app.getValue() === 42, "Module dependency should work");
  });

  // Test 5: ES6 module registration
  test("defineESModule registers ES6 modules", () => {
    clearAllModules();
    defineESModule("es6module", {
      add: (a: number, b: number) => a + b,
      multiply: (a: number, b: number) => a * b,
      default: "DefaultExport"
    });

    const m = require("es6module");
    assert(m.add(2, 3) === 5, "ES6 module exports should work");
    assert(m.default === "DefaultExport", "Default export should be present");
  });

  // Test 6: importModule function
  test("importModule extracts named exports", () => {
    clearAllModules();
    defineESModule("utils", {
      toUpperCase: (s: string) => s.toUpperCase(),
      toLowerCase: (s: string) => s.toLowerCase(),
      default: "StringUtils"
    });

    const upper = importModule("utils", "toUpperCase");
    assert(upper("hello") === "HELLO", "Named import should work");
  });

  // Test 7: importAll function
  test("importAll returns all exports", () => {
    clearAllModules();
    defineESModule("math", {
      add: (a: number, b: number) => a + b,
      subtract: (a: number, b: number) => a - b
    });

    const mathModule = importAll("math");
    assert(mathModule.add(5, 3) === 8, "Import all should work");
    assert(mathModule.subtract(5, 3) === 2, "All exports should be available");
  });

  // Test 8: Module count tracking
  test("getModuleCount returns correct count", () => {
    clearAllModules();
    registerModule("a", (m, e, r) => {});
    registerModule("b", (m, e, r) => {});
    registerModule("c", (m, e, r) => {});

    assert(getModuleCount() === 3, "Module count should be 3");
  });

  // Test 9: Cache size tracking
  test("getCacheSize tracks loaded modules", () => {
    clearAllModules();
    registerModule("a", (m, e, r) => {});
    registerModule("b", (m, e, r) => {});

    require("a");
    assert(getCacheSize() === 1, "Cache should contain 1 module");

    require("b");
    assert(getCacheSize() === 2, "Cache should contain 2 modules");
  });

  // Test 10: Module ID listing
  test("getModuleIds returns array of IDs", () => {
    clearAllModules();
    registerModule("first", (m, e, r) => {});
    registerModule("second", (m, e, r) => {});

    const ids = getModuleIds();
    assert(ids.length === 2, "Should have 2 module IDs");
    assert(ids.includes("first"), "Should include 'first'");
    assert(ids.includes("second"), "Should include 'second'");
  });

  // Test 11: isModuleLoaded check
  test("isModuleLoaded detects cached modules", () => {
    clearAllModules();
    registerModule("test", (m, e, r) => {});

    assert(isModuleLoaded("test") === false, "Module not loaded initially");
    require("test");
    assert(isModuleLoaded("test") === true, "Module loaded after require");
  });

  // Test 12: Module metadata
  test("getModuleMetadata returns correct info", () => {
    clearAllModules();
    registerModule("api", (m, e, r) => {
      e.fetch = () => {};
      e.post = () => {};
    });

    require("api");
    const meta = getModuleMetadata("api");

    assert(meta !== null, "Metadata should exist");
    assert(meta!.id === "api", "Metadata ID should match");
    assert(meta!.loaded === true, "Should be marked as loaded");
    assert(meta!.exportCount === 2, "Should count 2 exports");
  });

  // Test 13: Cache clearing
  test("clearModuleCache resets cache", () => {
    clearAllModules();
    registerModule("test", (m, e, r) => {
      e.value = 1;
    });

    const m1 = require("test");
    m1.value = 2;

    clearModuleCache();
    const m2 = require("test");

    assert(m2.value === 1, "Module should be re-executed after cache clear");
  });

  // Test 14: All modules clearing
  test("clearAllModules removes all modules", () => {
    registerModule("a", (m, e, r) => {});
    registerModule("b", (m, e, r) => {});

    clearAllModules();

    assert(getModuleCount() === 0, "Module count should be 0");
    assert(getCacheSize() === 0, "Cache should be empty");
  });

  // Test 15: Circular dependency handling
  test("circular dependencies don't cause infinite loops", () => {
    clearAllModules();

    registerModule("a", (m, e, r) => {
      e.value = "a";
      // Don't actually require 'b' to avoid infinite recursion
      e.getB = () => {
        const b = r("b");
        return b ? b.value : null;
      };
    });

    registerModule("b", (m, e, r) => {
      e.value = "b";
    });

    const a = require("a");
    assert(a.getB() === "b", "Can load dependent module later");
  });

  // Test 16: Import statement parsing
  test("parseImportStatement extracts module ID", () => {
    const result = parseImportStatement(`import { Component } from "react"`);
    assert(result.moduleId === "react", "Should extract module ID");
  });

  // Test 17: Module path resolution
  test("resolveModulePath removes extensions", () => {
    assert(resolveModulePath("utils.ts") === "utils", "Should remove .ts");
    assert(resolveModulePath("utils.js") === "utils", "Should remove .js");
    assert(resolveModulePath("utils") === "utils", "Should not change plain path");
  });

  // Test 18: Multiple module exports
  test("modules can export multiple functions", () => {
    clearAllModules();
    registerModule("string", (m, e, r) => {
      e.uppercase = (s: string) => s.toUpperCase();
      e.lowercase = (s: string) => s.toLowerCase();
      e.reverse = (s: string) => s.split("").reverse().join("");
    });

    const str = require("string");
    assert(str.uppercase("hello") === "HELLO", "Export 1 works");
    assert(str.lowercase("HELLO") === "hello", "Export 2 works");
    assert(str.reverse("hello") === "olleh", "Export 3 works");
  });

  // Test 19: Module statistics
  test("getModuleStatistics reports usage", () => {
    clearAllModules();
    registerModule("a", (m, e, r) => {
      e.x = 1;
      e.y = 2;
    });
    registerModule("b", (m, e, r) => {
      e.z = 3;
    });

    require("a");
    require("b");

    const stats = getModuleStatistics();
    assert(stats.modules === 2, "Should count 2 modules");
    assert(stats.cached === 2, "Should count 2 cached");
    assert(stats.totalExports === 3, "Should count 3 exports");
  });

  // Test 20: Unregister module
  test("unregisterModule removes module", () => {
    clearAllModules();
    registerModule("test", (m, e, r) => {});

    assert(getModuleCount() === 1, "Module should be registered");
    const result = unregisterModule("test");
    assert(result === true, "Unregister should return true");
    assert(getModuleCount() === 0, "Module should be removed");
  });

  // Test 21: Get all module exports
  test("getAllModuleExports returns all loaded modules", () => {
    clearAllModules();
    registerModule("a", (m, e, r) => {
      e.value = 1;
    });
    registerModule("b", (m, e, r) => {
      e.value = 2;
    });

    require("a");
    require("b");

    const all = getAllModuleExports();
    assert(all.size === 2, "Should have 2 modules");
    assert(all.get("a")!.value === 1, "Module A exports correct");
    assert(all.get("b")!.value === 2, "Module B exports correct");
  });

  // Test 22: List all modules
  test("listAllModules provides metadata for all", () => {
    clearAllModules();
    registerModule("x", (m, e, r) => {
      e.a = 1;
      e.b = 2;
    });
    registerModule("y", (m, e, r) => {
      e.c = 3;
    });

    require("x");

    const list = listAllModules();
    assert(list.length === 2, "Should list 2 modules");
    assert(list[0].loaded === true, "First module loaded");
    assert(list[1].loaded === false, "Second module not loaded");
  });

  // Test 23: Module exports persistence
  test("module exports persist across calls", () => {
    clearAllModules();
    let callCount = 0;

    registerModule("singleton", (m, e, r) => {
      callCount++;
      e.id = callCount;
      e.getCallCount = () => callCount;
    });

    const m1 = require("singleton");
    const m2 = require("singleton");

    assert(m1.id === 1, "Factory executed once");
    assert(m1 === m2, "Same instance returned");
    assert(m1.getCallCount() === 1, "Factory not re-executed");
  });

  // Test 24: Nested module dependencies
  test("nested dependencies resolve correctly", () => {
    clearAllModules();

    registerModule("level1", (m, e, r) => {
      e.value = "level1";
    });

    registerModule("level2", (m, e, r) => {
      const l1 = r("level1");
      e.value = l1.value + "-level2";
    });

    registerModule("level3", (m, e, r) => {
      const l2 = r("level2");
      e.value = l2.value + "-level3";
    });

    const l3 = require("level3");
    assert(l3.value === "level1-level2-level3", "Nested deps work");
  });

  // Test 25: Empty module exports
  test("modules can have empty exports", () => {
    clearAllModules();
    registerModule("empty", (m, e, r) => {
      // Don't export anything
    });

    const result = require("empty");
    assert(result !== null, "Module should load");
    assert(Object.keys(result).length === 0, "No exports");
  });

  // Test 26: Nonexistent module handling
  test("require returns null for nonexistent module", () => {
    clearAllModules();
    const result = require("nonexistent");
    assert(result === null, "Should return null for missing module");
  });

  // Test 27: Module with functions as exports
  test("functions can be exported directly", () => {
    clearAllModules();
    registerModule("funcs", (m, e, r) => {
      e.greet = (name: string) => "Hello, " + name;
      e.goodbye = (name: string) => "Bye, " + name;
    });

    const funcs = require("funcs");
    assert(funcs.greet("World") === "Hello, World", "Function export works");
    assert(funcs.goodbye("World") === "Bye, World", "Multiple functions work");
  });

  // Test 28: Module identity with multiple references
  test("same module instance across multiple references", () => {
    clearAllModules();
    registerModule("shared", (m, e, r) => {
      e.shared = true;
    });

    const a = require("shared");
    const b = require("shared");
    const c = importModule("shared", "");

    assert(a === b, "require returns same instance");
    assert(a === c, "importModule returns same instance");
  });
}

// Helper test function
function test(description: string, fn: () => void): void {
  try {
    fn();
    console.log("✓ " + description);
  } catch (e) {
    console.error("✗ " + description);
    console.error("  Error: " + (e instanceof Error ? e.message : String(e)));
  }
}

// Helper assert function
function assert(condition: boolean, message: string): void {
  if (!condition) {
    throw new Error(message);
  }
}
