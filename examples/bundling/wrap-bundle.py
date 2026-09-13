#!/usr/bin/env python3
"""
Bundle Wrapper Generator for Artisan

Takes a bundled JavaScript file and generates an ART/TypeScript module wrapper
that registers it with the Artisan module system.

Usage:
    python3 wrap-bundle.py <bundle-file> <module-id> [output-file]

Example:
    python3 wrap-bundle.py bundles/angular.js "@angular/core" > src/angular-module.ts
    python3 wrap-bundle.py bundles/react-router.js "react-router-dom" --out src/router.ts
"""

import sys
import os
from pathlib import Path


def sanitize_module_id(module_id: str) -> str:
    """Convert module ID to valid TypeScript identifier"""
    # Replace special characters
    sanitized = module_id.replace("@", "").replace("/", "_").replace("-", "_")
    # Ensure starts with letter
    if sanitized[0].isdigit():
        sanitized = "_" + sanitized
    return sanitized


def read_bundle_file(filepath: str) -> str:
    """Read bundle file contents"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            return f.read()
    except FileNotFoundError:
        print(f"Error: Bundle file not found: {filepath}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error reading bundle file: {e}", file=sys.stderr)
        sys.exit(1)


def escape_code_for_typescript(code: str) -> str:
    """Escape JavaScript code for embedding in TypeScript string"""
    # Escape backslashes first
    code = code.replace('\\', '\\\\')
    # Escape backticks (template literal delimiter)
    code = code.replace('`', '\\`')
    # Escape dollar signs used in template literals
    code = code.replace('${', '\\${')
    return code


def create_module_wrapper(bundle_file: str, module_id: str) -> str:
    """Generate ART/TypeScript module wrapper"""

    # Read bundle
    bundle_code = read_bundle_file(bundle_file)

    # Generate safe function name
    safe_name = sanitize_module_id(module_id)
    function_name = f"register{safe_name.capitalize()}"

    # Escape the code
    escaped_code = escape_code_for_typescript(bundle_code)

    # Generate wrapper
    wrapper = f'''// Auto-generated module wrapper for {module_id}
// Generated from: {os.path.basename(bundle_file)}
// Do not edit manually

import {{ registerModule }} from "art/modules";

/**
 * Register the {module_id} module with the Artisan module system
 *
 * This function embeds the bundled {module_id} code and makes it available
 * via the require() function:
 *
 *   const mod = require("{module_id}");
 */
export function {function_name}(): void {{
  registerModule("{module_id}", function(module, exports, require) {{
    // Bundled {module_id} code
    const bundledCode = `{escaped_code}`;

    // Execute the bundled code in this module's context
    // The bundle should set window/global variables with the exports
    eval(bundledCode);

    // Re-export what the bundle provides
    // If the bundle uses a specific global variable name:
    // (Adjust these based on what your esbuild output provides)
    if (typeof {safe_name} !== "undefined") {{
      Object.assign(exports, {safe_name});
    }}
  }});
}}

// Also export a convenience function to check if module is available
export function is{safe_name.capitalize()}Available(): boolean {{
  try {{
    const mod = require("{module_id}");
    return mod !== null && mod !== undefined;
  }} catch {{
    return false;
  }}
}}
'''

    return wrapper


def create_multi_bundle_wrapper(bundles: list) -> str:
    """Generate wrapper for multiple bundles"""

    imports = []
    calls = []

    for bundle_file, module_id in bundles:
        safe_name = sanitize_module_id(module_id)
        function_name = f"register{safe_name.capitalize()}"
        imports.append(f"import {{ {function_name} }} from \"./modules/{safe_name}\";")
        calls.append(f"  {function_name}();")

    wrapper = f'''// Auto-generated: Register multiple bundled modules

{chr(10).join(imports)}

/**
 * Register all bundled modules with the Artisan module system
 */
export function registerAllBundles(): void {{
{chr(10).join(calls)}
}}
'''

    return wrapper


def main():
    """Main entry point"""

    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    bundle_file = sys.argv[1]
    module_id = sys.argv[2]
    output_file = None

    # Check for --out flag
    if len(sys.argv) > 3 and sys.argv[3] == "--out" and len(sys.argv) > 4:
        output_file = sys.argv[4]

    # Generate wrapper
    wrapper = create_module_wrapper(bundle_file, module_id)

    # Output
    if output_file:
        try:
            # Create directory if needed
            os.makedirs(os.path.dirname(output_file), exist_ok=True)

            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(wrapper)
            print(f"✓ Generated: {output_file}", file=sys.stderr)
        except Exception as e:
            print(f"Error writing output file: {e}", file=sys.stderr)
            sys.exit(1)
    else:
        # Write to stdout
        print(wrapper)


if __name__ == "__main__":
    main()
