# Artisan Project Structure

Understanding the layout and files in an Artisan project.

## Basic Project Layout

```
my-app/
  ├── pages/
  │   ├── index.html           # Home page
  │   ├── about.html           # About page
  │   └── settings/
  │       └── profile.html     # Nested page (route: /settings/profile)
  │
  ├── app.tsx                  # Main ART code (optional)
  ├── app.jsx                  # Main React code (optional)
  │
  ├── package.json            # NPM dependencies (optional)
  └── tsconfig.json           # TypeScript/ART config (optional)
```

## File Types

### Pages (Required - at least one)

**Location:** `pages/**/*.html`

Each HTML file becomes a page in your app. Folder structure maps to routes:

- `pages/index.html` → route `/`
- `pages/about.html` → route `/about`
- `pages/settings/profile.html` → route `/settings/profile`
- `pages/settings/index.html` → route `/settings`

**Minimal page:**
```html
<!DOCTYPE html>
<html>
  <head>
    <title>My App</title>
  </head>
  <body>
    <div id="root"></div>
  </body>
</html>
```

The `<div id="root">` is where your ART or JavaScript code mounts.

### ART Code (Optional)

**Location:** `app.tsx` (or `app.ts`)

The main ART application file. Runs once per page load.

```typescript
// app.tsx
import { Node, Event } from "art";

function onButtonClick(event: Event): void {
  // Handle click
}

let button: Node = document.getElementById("my-button");
if (!button.isNull()) {
  button.addEventListener("click", onButtonClick, false);
}
```

**Why `app.tsx`?**
- `.tsx` means "TypeScript + JSX"
- Allows using JSX syntax in ART
- If you don't use JSX, `.ts` works too

### JavaScript Code (Optional)

**Location:** `app.js` or `app.jsx`

JavaScript/React code that runs at startup.

```javascript
// app.jsx
import React from "react";
import ReactDOM from "react-dom/client";

function App() {
  return <h1>Hello React!</h1>;
}

ReactDOM.createRoot(document.getElementById("root")).render(<App />);
```

### Configuration Files (Optional)

**`package.json`** - NPM dependencies
```json
{
  "name": "my-app",
  "version": "1.0.0",
  "dependencies": {
    "react": "^18.3.1",
    "react-dom": "^18.3.1"
  }
}
```

**`tsconfig.json`** - TypeScript/ART configuration
```json
{
  "compilerOptions": {
    "target": "ES2020",
    "module": "commonjs",
    "strict": true
  }
}
```

## What Gets Built?

The build process discovers and compiles:

1. **All HTML files** in `pages/` → merged into widget tree
2. **`app.tsx`** (if present) → compiled to machine code
3. **`app.jsx`/`app.js`** (if present) → embedded in binary, run at startup
4. **Dependencies** in `package.json` (if bundled)

## Build Output

After `artisan-cli build my-app`:

```
build/
  ├── my-app              # Final executable
  └── CMakeFiles/         # Build artifacts
```

Run the app:
```bash
./build/my-app
```

## File Discovery Rules

- **Pages:** All `.html` files in `pages/` directory (recursively)
- **ART app:** Single file named `app.tsx` or `app.ts` at project root
- **JavaScript app:** Single file named `app.js` or `app.jsx` at project root
- **Modules:** Any `.ts`/`.tsx` files imported by `app.tsx` (for ART)

## Multi-File ART Projects

Split your ART code across multiple files using imports:

```
my-app/
  ├── pages/
  │   └── index.html
  ├── app.tsx           # Entry point
  └── lib/
      ├── ui.ts
      └── utils.ts
```

**`app.tsx`:**
```typescript
import { setupUI } from "./lib/ui";
import { utils } from "./lib/utils";

setupUI();
```

**`lib/ui.ts`:**
```typescript
import { Node } from "art";

export function setupUI(): void {
  // Build UI
}
```

**`lib/utils.ts`:**
```typescript
export function add(a: number, b: number): number {
  return a + b;
}
```

## Multi-File JavaScript Projects

Use the module system for JavaScript:

```
my-app/
  ├── pages/
  │   └── index.html
  ├── app.jsx           # Entry point
  └── modules/
      ├── Counter.jsx
      └── Button.jsx
```

**`app.jsx`:**
```javascript
import { registerModule, require } from "art/modules";

// Register modules
registerModule("components/Counter", () => {
  // Import Counter component
});

const React = require("react");
const Counter = require("components/Counter");

// Use Counter
```

See [Module System Guide](MODULES.md) for details.

## Adding Static Assets

Put assets in a directory (they're not automatically discovered):

```
my-app/
  ├── pages/
  ├── assets/
  │   ├── images/
  │   ├── fonts/
  │   └── data.json
  └── app.tsx
```

Access from ART/JavaScript:
```typescript
// ART
let data = /* read from file at build time */

// JavaScript
fetch("/assets/data.json").then(r => r.json());
```

Assets are embedded in the binary or served from the filesystem.

## Debugging Project Structure Issues

### "Module not found"

Check that imports use correct paths:
```typescript
// ✓ Correct
import { setupUI } from "./lib/ui";
import { Button } from "../components/Button";

// ✗ Wrong
import { setupUI } from "lib/ui";  // Missing ./
```

### "Page not rendering"

Check that:
1. HTML file is in `pages/` directory
2. HTML has a `<div id="root">` or similar mount point
3. ART/JavaScript code targets the correct ID:
   ```typescript
   let root: Node = document.getElementById("root");
   ```

### "Build fails with multiple app files"

The build expects only one:
- One `app.tsx` OR one `app.ts` (not both)
- One `app.jsx` OR one `app.js` (not both)
- Delete duplicates

## Tips

1. **Keep pages simple** - HTML should mostly be markup
2. **Put logic in app.tsx/app.jsx** - Application code goes there
3. **Use modules for large projects** - Split code into logical units
4. **One entry point** - Only `app.tsx` or `app.jsx` runs automatically
5. **Test incrementally** - Build frequently to catch issues early

---

Ready to start building? → [Getting Started](GETTING_STARTED.md)
