#!/bin/bash

# Bundle npm libraries for the Task Manager app
# Run: bash bundle.sh

set -e

echo "=== Task Manager - Bundling npm Libraries ==="

# Install dependencies if needed
if [ ! -d "node_modules" ]; then
  echo "Installing dependencies..."
  npm install
fi

# Create bundles directory
mkdir -p bundles

# Bundle libraries with esbuild
echo "Bundling lodash and date-fns..."
npx esbuild bundle-libs.js \
  --bundle \
  --format=iife \
  --platform=neutral \
  --minify \
  --outfile=bundles/libs.js

echo "✓ Bundled: bundles/libs.js"
echo ""
echo "Bundle stats:"
ls -lh bundles/libs.js
echo ""
echo "Next steps:"
echo "1. The bundled code is in bundles/libs.js"
echo "2. Wrap it with Python: python3 ../wrap-bundle.py bundles/libs.js 'task-libs' > modules/libs-module.ts"
echo "3. Or use it directly in app.jsx by embedding the bundle"
