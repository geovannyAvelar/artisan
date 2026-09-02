#!/usr/bin/env bash
# ART's regression suite - compiles (and, where possible, runs) every
# test program here against the standalone `art` compiler, reporting
# pass/fail for each. Build `art` first if you haven't:
#
#   cmake -S art -B art/build -GNinja && cmake --build art/build
#
# Usage: art/tests/run.sh [path/to/art-binary]
# (defaults to art/build/art, next to this checkout)
#
# Three kinds of test, by location:
#   art/tests/*.ts                     compiled, linked, and RUN -
#                                       main(): number must return 0.
#                                       Never touches the DOM/timers -
#                                       needs nothing beyond `art`
#                                       itself (i.e. LLVM), so this is
#                                       real, working-binary proof, not
#                                       just "it compiled".
#   art/tests/<name>/app.ts[x]         same, but a multi-file program -
#                                       every other file in <name>/ is
#                                       something app.ts[x] imports.
#   art/tests/*.tsx (dom_jsx_timers)   COMPILE-ONLY (--emit-obj) - these
#                                       use the DOM/timer bridge
#                                       (`import ... from "art"`), which
#                                       needs the real bridge object
#                                       files plus libgc/SDL2 to actually
#                                       link and run (see the harnesses
#                                       under art/README.md's own
#                                       verification notes) - out of
#                                       scope for this script, so these
#                                       only prove the tokenizer/parser/
#                                       Sema/Codegen accept the program,
#                                       not that it behaves correctly at
#                                       runtime.
#   art/tests/errors/*.ts[x]           compiled with --emit-obj,
#                                       expected to FAIL - a real
#                                       language mistake correctly
#                                       rejected, not a regression.
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ART="${1:-$SCRIPT_DIR/../build/art}"

if [[ ! -x "$ART" ]]; then
  echo "error: '$ART' not found or not executable - build it first:"
  echo "  cmake -S art -B art/build -GNinja && cmake --build art/build"
  exit 1
fi

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

pass=0
fail=0

# Compiles $1, links it into a real binary, and runs it - expects exit
# code 0 (each test's own main(): number reports failures by returning
# a nonzero count, so a nonzero exit here always names a real failure).
run_positive_test() {
  local file="$1" name bin
  name="$(basename "$(dirname "$file")")/$(basename "$file")"
  bin="$WORKDIR/$(basename "$file").bin"
  if ! "$ART" "$file" -o "$bin" >"$WORKDIR/log" 2>&1; then
    echo "FAIL (compile) $name"
    sed 's/^/    /' "$WORKDIR/log"
    fail=$((fail + 1))
    return
  fi
  if ! "$bin" >"$WORKDIR/run.log" 2>&1; then
    echo "FAIL (exit code) $name"
    sed 's/^/    /' "$WORKDIR/run.log"
    fail=$((fail + 1))
    return
  fi
  echo "ok   $name"
  pass=$((pass + 1))
}

# Compiles $1 with --emit-obj only - expects success, but never links or
# runs it (see this script's own header comment for why).
run_compile_only_test() {
  local file="$1" name obj
  name="$(basename "$file")"
  obj="$WORKDIR/$name.o"
  if ! "$ART" "$file" --emit-obj -o "$obj" >"$WORKDIR/log" 2>&1; then
    echo "FAIL (compile) $name"
    sed 's/^/    /' "$WORKDIR/log"
    fail=$((fail + 1))
    return
  fi
  echo "ok   $name (compile-only)"
  pass=$((pass + 1))
}

# Compiles $1 with --emit-obj - expects it to FAIL. A test here that
# compiles successfully is itself the failure (the mistake it's supposed
# to demonstrate went uncaught).
run_error_test() {
  local file="$1" name obj
  name="errors/$(basename "$file")"
  obj="$WORKDIR/$(basename "$file").o"
  if "$ART" "$file" --emit-obj -o "$obj" >"$WORKDIR/log" 2>&1; then
    echo "FAIL (expected a compile error, got none) $name"
    fail=$((fail + 1))
    return
  fi
  echo "ok   $name (correctly rejected)"
  pass=$((pass + 1))
}

for file in "$SCRIPT_DIR"/*.ts; do
  [[ -e "$file" ]] || continue
  run_positive_test "$file"
done

for file in "$SCRIPT_DIR"/*.tsx; do
  [[ -e "$file" ]] || continue
  run_compile_only_test "$file"
done

for dir in "$SCRIPT_DIR"/*/; do
  dir="${dir%/}"
  base="$(basename "$dir")"
  [[ "$base" == "errors" ]] && continue
  if [[ -e "$dir/app.ts" ]]; then
    run_positive_test "$dir/app.ts"
  elif [[ -e "$dir/app.tsx" ]]; then
    run_positive_test "$dir/app.tsx"
  fi
done

for file in "$SCRIPT_DIR"/errors/*.ts "$SCRIPT_DIR"/errors/*.tsx; do
  [[ -e "$file" ]] || continue
  run_error_test "$file"
done

echo
echo "$pass passed, $fail failed"
[[ "$fail" -eq 0 ]]
