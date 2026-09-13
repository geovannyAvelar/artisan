#!/bin/bash
# Angular Bundling Script for Artisan
# Creates bundles for RxJS and Angular packages

set -e

BUNDLE_DIR="./bundles"
TEMP_DIR="/tmp/artisan-bundles"

echo "=== Artisan Angular Bundling ==="
echo ""

# Create directories
mkdir -p "$BUNDLE_DIR"
mkdir -p "$TEMP_DIR"

# Check esbuild
if ! command -v esbuild &> /dev/null; then
  echo "Error: esbuild not found. Install with: npm install -g esbuild"
  exit 1
fi

echo "✓ esbuild found: $(esbuild --version)"
echo ""

# Create bundle entry points
echo "Creating bundle entry points..."

# RxJS bundle
cat > "$TEMP_DIR/bundle-rxjs.js" << 'EOF'
export * from "rxjs";
export * from "rxjs/operators";
EOF
echo "  ✓ bundle-rxjs.js"

# Angular core
cat > "$TEMP_DIR/bundle-angular-core.js" << 'EOF'
export { default as angular } from "@angular/core";
export * from "@angular/core";
EOF
echo "  ✓ bundle-angular-core.js"

# Angular full (with common and forms)
cat > "$TEMP_DIR/bundle-angular-full.js" << 'EOF'
export * from "@angular/core";
export * from "@angular/common";
export * from "@angular/forms";
export * from "@angular/platform-browser";
EOF
echo "  ✓ bundle-angular-full.js"

echo ""
echo "Bundling with esbuild..."
echo ""

# Bundle RxJS
echo "Bundling RxJS..."
esbuild "$TEMP_DIR/bundle-rxjs.js" \
  --bundle \
  --format=iife \
  --minify \
  --outfile="$BUNDLE_DIR/rxjs.js" \
  2>/dev/null || {
    echo "Warning: RxJS bundling may have issues. Trying without minify..."
    esbuild "$TEMP_DIR/bundle-rxjs.js" \
      --bundle \
      --format=iife \
      --outfile="$BUNDLE_DIR/rxjs.js"
  }
echo "  ✓ $BUNDLE_DIR/rxjs.js"

# Bundle Angular Core
echo "Bundling Angular Core..."
esbuild "$TEMP_DIR/bundle-angular-core.js" \
  --bundle \
  --format=iife \
  --external:rxjs \
  --external:zone.js \
  --minify \
  --outfile="$BUNDLE_DIR/angular-core.js" \
  2>/dev/null || {
    echo "Warning: Angular bundling may have issues. Trying without minify..."
    esbuild "$TEMP_DIR/bundle-angular-core.js" \
      --bundle \
      --format=iife \
      --external:rxjs \
      --external:zone.js \
      --outfile="$BUNDLE_DIR/angular-core.js"
  }
echo "  ✓ $BUNDLE_DIR/angular-core.js"

# Bundle Angular Full
echo "Bundling Angular Full..."
esbuild "$TEMP_DIR/bundle-angular-full.js" \
  --bundle \
  --format=iife \
  --external:rxjs \
  --external:zone.js \
  --minify \
  --outfile="$BUNDLE_DIR/angular-full.js" \
  2>/dev/null || {
    echo "Warning: Angular full bundling may have issues. Trying without minify..."
    esbuild "$TEMP_DIR/bundle-angular-full.js" \
      --bundle \
      --format=iife \
      --external:rxjs \
      --external:zone.js \
      --outfile="$BUNDLE_DIR/angular-full.js"
  }
echo "  ✓ $BUNDLE_DIR/angular-full.js"

echo ""
echo "=== Bundling Complete ==="
echo ""
echo "Bundle sizes:"
ls -lh "$BUNDLE_DIR/" | tail -3 | awk '{print "  " $9 ": " $5}'

echo ""
echo "Next steps:"
echo "  1. Review the bundles in $BUNDLE_DIR/"
echo "  2. Run wrap-bundle.py to generate module wrappers"
echo "  3. Check app.ts for usage examples"
echo ""

# Cleanup
rm -rf "$TEMP_DIR"
