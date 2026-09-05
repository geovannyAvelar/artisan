#!/usr/bin/env bash

cd third_party/skia

python3 tools/git-sync-deps

# Upstream Skia's own third_party/BUILD.gn has no way to point
# skia_use_fontconfig at a non-default fontconfig include path - needed on
# macOS, where fontconfig comes from Homebrew, not a system location GN
# would find on its own. There's no push access to Google's own Skia repo
# to upstream this, so it's kept as a small local patch instead (applied
# idempotently here - a no-op once already applied, e.g. on a rebuild).
if git apply --check "../skia-fontconfig-include-path.patch" 2>/dev/null; then
  git apply "../skia-fontconfig-include-path.patch"
fi

FREETYPE_INCLUDE_DIR="$(pkg-config --cflags-only-I freetype2 | tr ' ' '\n' | grep freetype2 | head -1 | sed 's/^-I//')"
FONTCONFIG_INCLUDE_DIR="$(pkg-config --cflags-only-I fontconfig | tr ' ' '\n' | head -1 | sed 's/^-I//')"

bin/gn gen out/Debug --args="skia_use_freetype=true skia_use_fontconfig=true skia_use_fonthost_mac=false skia_system_freetype2_include_path=\"${FREETYPE_INCLUDE_DIR}\" skia_system_fontconfig_include_path=\"${FONTCONFIG_INCLUDE_DIR}\""
ninja -C out/Debug skia
