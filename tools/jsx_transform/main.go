// jsx_transform is a build-time-only tool, independent of the ART
// compiler: it turns one .jsx file (real JSX, real loosely-typed
// JavaScript - not ART's own restricted, statically typed JSX) into
// plain JS with every JSX element literal rewritten to an h(...)/
// Fragment call - h/Fragment being the framework-agnostic, always-
// native JSX target js_engine.cpp provides (see its own "h/Fragment"
// section), not React or any other UI library specifically. Uses
// esbuild's own JSX transform (github.com/evanw/esbuild) rather than a
// hand-written parser - the classic-runtime pragma (JSXFactory/
// JSXFragment below), the same mechanism React's own classic transform
// uses, just pointed at h/Fragment instead - so the output only ever
// needs those two (always-present) globals, never a separate
// jsx-runtime import. A project wanting a real UI library (React or
// otherwise) instead of the built-in h/Fragment reassigns those two
// globals itself at runtime (see README.md's "Using JavaScript"
// section) - this tool's own pragma target never changes.
//
// Everything that isn't a JSX element literal passes through
// unchanged - this is a JSX-only transform, not a bundler: it doesn't
// resolve imports, doesn't touch modules, and doesn't optimize
// anything. Mirrors src/tools/embed_text.cpp's single-purpose CLI
// shape: two positional arguments, input path then output path.
package main

import (
	"fmt"
	"os"

	"github.com/evanw/esbuild/pkg/api"
)

func main() {
	if len(os.Args) != 3 {
		fmt.Fprintln(os.Stderr, "usage: jsx_transform <input.jsx> <output.js>")
		os.Exit(1)
	}
	inputPath := os.Args[1]
	outputPath := os.Args[2]

	source, err := os.ReadFile(inputPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "jsx_transform: reading %s: %v\n", inputPath, err)
		os.Exit(1)
	}

	result := api.Transform(string(source), api.TransformOptions{
		Loader:      api.LoaderJSX,
		JSX:         api.JSXTransform, // classic runtime - h/Fragment as globals, no jsx-runtime import
		JSXFactory:  "h",
		JSXFragment: "Fragment",
		Sourcefile:  inputPath,
		LogLevel:    api.LogLevelSilent,
	})

	if len(result.Errors) > 0 {
		for _, e := range result.Errors {
			fmt.Fprintf(os.Stderr, "jsx_transform: %s\n", e.Text)
		}
		os.Exit(1)
	}

	if err := os.WriteFile(outputPath, result.Code, 0644); err != nil {
		fmt.Fprintf(os.Stderr, "jsx_transform: writing %s: %v\n", outputPath, err)
		os.Exit(1)
	}
}
