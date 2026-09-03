#pragma once

namespace artisan {

// Defined by a generated translation unit: the contents of whichever .js
// file the build was configured with (ARTISAN_APP_JS_SOURCE in
// CMakeLists.txt / --js in artisan-cli), embedded as a plain C++ string
// at build time - or "" if no script file was given. Either way, nothing
// reads this file at runtime; by the time the app runs, its text already
// lives in the binary's data section.
const char *GetAppScript();

// Same pattern as GetAppScript(), for a second independent optional
// script (ARTISAN_JS_PRELUDE_SOURCE) run before it - see src/main.cpp's
// navigate(). E.g. a vendored UI library (see third_party/react/ for a
// ready-made example) a project wants as a global before its own app
// script runs - not tied to any particular library, same
// "" -if-unconfigured contract as GetAppScript().
const char *GetJsPreludeScript();

} // namespace artisan
