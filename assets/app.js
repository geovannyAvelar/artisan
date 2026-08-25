var nameInput = document.getElementById("name-input");
var greeting = document.getElementById("greeting");

document.getElementById("submit-button").addEventListener("click", function () {
  var name = nameInput.getAttribute("value") || "";
  if (name === "") {
    greeting.textContent = "Please enter a name first.";
  } else {
    greeting.textContent = "Hello, " + name + "!";
  }
});
