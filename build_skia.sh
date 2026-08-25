#!/usr/bin/env bash

cd third_party/skia

python3 tools/git-sync-deps

bin/gn gen out/Debug
ninja -C out/Debug skia
