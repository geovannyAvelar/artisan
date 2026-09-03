// This repo's own demo - the ART counterpart to assets/app.js (Submit is
// wired up there, through QuickJS; Clear is wired up here, compiled
// ahead of time with the ART compiler) - both find the same nodes by id
// in assets/index.html and drive them through the identical Node API.
import { Node, Event } from "art";

function onClearClick(event: Event): void {
  let nameInput: Node = document.getElementById("name-input");
  let emailInput: Node = document.getElementById("email-input");
  let greeting: Node = document.getElementById("greeting");
  if (!nameInput.isNull() && !emailInput.isNull() && !greeting.isNull()) {
    nameInput.setAttribute("value", "");
    emailInput.setAttribute("value", "");
    greeting.textContent = "Fill in your name and click Submit.";
  }
}

let clearButton: Node = document.getElementById("clear-button");
if (!clearButton.isNull()) {
  clearButton.addEventListener("click", onClearClick, false);
}
