// A top-level `let`/`const` whose initializer touches `document` is no
// longer a compile error requiring a manual `{ }` wrapper - it's
// automatically treated as a per-page-load local instead (see
// Parser::ParseProgram's use of kAmbientGlobals/
// ExprMayReferenceAmbientGlobal), exactly as if it HAD been wrapped.
// This is the bare, unwrapped form - no `{ }` anywhere in this file.
// Compile-only (see run.sh's header comment) - a real Click()-dispatch
// check for this exact pattern already ran during development (see the
// session's own harness notes); this still catches a parser/Sema/
// Codegen-level regression even without a live DOM to run it against.
import { Node, Event } from "art";

let clickCount: number = 0;

function onButtonClick(event: Event): void {
  clickCount++;
  let label: Node = document.getElementById("count-label");
  if (!label.isNull()) {
    label.textContent = `Clicked ${numberToString(clickCount)} times`;
  }
}

let root: Node = document.getElementById("root");
if (!root.isNull()) {
  root.appendChild(
    <div>
      <p id="count-label">{"Clicked 0 times"}</p>
      <button onclick={onButtonClick}>{"Click me"}</button>
    </div>
  );
}
