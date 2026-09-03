// Closures crossing the native DOM/timer bridge - addEventListener, the
// JSX onclick={...} sugar (a second, hand-rolled call site that needs
// the same {fn,env}-unpacking rule as an ordinary call - see
// Codegen::GenExpr's JsxElement case), and setTimeout, all with a real
// captured environment. Compile-only (see run.sh's own header comment)
// - a live Click()-dispatch harness proving these actually run
// correctly at runtime lives outside this repo's checked-in test suite,
// same convention art/README.md already documents for DOM/event
// behavior.

import { Node, Event, setTimeout } from "art";

{
  let clickCount: number = 0;
  let label: Node = document.getElementById("count-label");

  let onClick: (event: Event) => void = function(event: Event): void {
    clickCount = clickCount + 1;
    if (!label.isNull()) {
      label.textContent = numberToString(clickCount);
    }
  };

  let button: Node = document.getElementById("increment-button");
  if (!button.isNull()) {
    button.addEventListener("click", onClick, false);
    button.removeEventListener("click", onClick, false);
    button.addEventListener("click", onClick, false);
  }

  let root: Node = document.getElementById("root");
  if (!root.isNull()) {
    root.appendChild(<button onclick={onClick}>{"Also increments"}</button>);
  }

  let delayMs: number = 100;
  setTimeout(function(): void { clickCount = clickCount + 1; }, delayMs);
}
