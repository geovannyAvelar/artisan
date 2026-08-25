#pragma once

namespace artisan {

// Defined by a generated translation unit: the contents of whichever .js
// file the build was configured with (ARTISAN_APP_JS_SOURCE in
// CMakeLists.txt / --js in artisan-cli), embedded as a plain C++ string
// at build time - or "" if no script file was given. Either way, nothing
// reads this file at runtime; by the time the app runs, its text already
// lives in the binary's data section.
const char *GetAppScript();

} // namespace artisan
