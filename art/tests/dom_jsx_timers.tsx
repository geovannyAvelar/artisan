// DOM/JSX/timers: Node/Event, classList/style, JSX elements/fragments/
// Node[] spreading, and the timer functions - all from ART's standard
// library ("art"). Compile-only (see run.sh): linking and actually
// running this needs the real DOM/timer bridge object files and libgc/
// SDL2, which run.sh doesn't set up - this still catches a tokenizer/
// parser/Sema/Codegen-level regression (a crash, a wrong type, an LLVM
// verification failure) even without a live DOM to run it against.

import { Node, Event, setTimeout, setInterval, clearInterval, requestAnimationFrame } from "art";

function onItemClick(event: Event): void {
  let item: Node = event.target;
  let wasSelected: boolean = item.classListContains("selected");
  item.classListToggle("selected", false, false);
  item.setStyle("color", wasSelected ? "" : "blue");
}

function makeItem(label: string, index: number): Node {
  return <li class={`item item-${index}`} data-index={numberToString(index)} onclick={onItemClick}>{label}</li>;
}

function buildItems(labels: string[]): Node[] {
  let items: Node[] = makeArray::<Node>(labels.length, <li></li>);
  let i: number = 0;
  while (i < labels.length) {
    items[i] = makeItem(labels[i], i);
    i = i + 1;
  }
  return items;
}

function headerAndFooter(): Node[] {
  return <>
    <li class="header">{"header"}</li>
    <li class="footer">{"footer"}</li>
  </>;
}

function renderList(labels: string[]): Node {
  return <ul>{headerAndFooter()}{buildItems(labels)}</ul>;
}

function onTick(): void {}
function onTimeout(): void {}
function onFrame(timestamp: number): void {}

{
  let root: Node = document.getElementById("root");
  if (!root.isNull()) {
    root.appendChild(renderList(["a", "b", "c"]));
  }

  let intervalId: number = setInterval(onTick, 1000);
  setTimeout(onTimeout, 5000);
  clearInterval(intervalId);
  requestAnimationFrame(onFrame);
}
