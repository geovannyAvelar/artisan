#!/usr/bin/env bash
# Builds a .deb of artisan-cli, entirely locally - no CI, no publishing.
#
# artisan-cli isn't self-contained (see CMakeLists.txt's packaging
# section): every `artisan-cli build` re-invokes cmake against this whole
# checkout, Skia's prebuilt static libraries included, so the .deb bundles
# a copy of the tree rather than just the binary. That makes for a large
# package (mostly Skia's .a files) - run ./build_skia.sh first if you
# haven't, this script only packages what's already built.
#
# The version embeds the current commit so repeated local builds are
# distinguishable, e.g. 0.0.1~git20260825.abc1234 - not tied to any
# release/publishing process, just a label.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

SKIA_OUT="third_party/skia/out/Debug"
if ! compgen -G "${SKIA_OUT}/*.a" >/dev/null; then
  echo "build_deb.sh: no Skia static libraries in ${SKIA_OUT} - run ./build_skia.sh first" >&2
  exit 1
fi

BASE_VERSION=$(grep -m1 -oP '(?<=VERSION )[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt)
VERSION="${BASE_VERSION}~git$(date -u +%Y%m%d).$(git rev-parse --short HEAD)"

BUILD_DIR="build-deb"

echo "build_deb.sh: configuring (version ${VERSION})"
cmake -S . -B "${BUILD_DIR}" \
  -DARTISAN_PROJECT_SOURCE_DIR_OVERRIDE=/usr/lib/artisan \
  -DCPACK_PACKAGE_VERSION="${VERSION}"

echo "build_deb.sh: building artisan-cli"
cmake --build "${BUILD_DIR}" --target artisan_cli

echo "build_deb.sh: packaging"
(cd "${BUILD_DIR}" && cpack -G DEB)

mkdir -p dist
DEB_FILE=$(find "${BUILD_DIR}" -maxdepth 1 -name "*.deb" | head -n1)
cp "${DEB_FILE}" dist/

echo "build_deb.sh: built dist/$(basename "${DEB_FILE}")"
