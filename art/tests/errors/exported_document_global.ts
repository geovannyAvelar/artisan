// Expected error: an exported top-level global still can't touch
// `document` - exports promise a real, cross-file-visible persistent
// global (initialized once, at process start, before any page has ever
// loaded), and a document-touching declaration can only ever be a
// per-page-load local (see Parser::ParseProgram/kAmbientGlobals),
// which can't be exported. Non-exported document-touching globals are
// no longer an error at all (see ../no_wrapper_document.tsx) - `export`
// is what still makes this one illegal.
import { Node } from "art";

export let root: Node = document.getElementById("root");
