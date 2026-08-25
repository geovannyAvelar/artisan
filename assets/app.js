var nameInput = document.getElementById("name-input");
var greeting = document.getElementById("greeting");
var submitButton = document.getElementById("submit-button");

if (nameInput && greeting && submitButton) {
  submitButton.addEventListener("click", function () {
    var name = nameInput.getAttribute("value") || "";
    if (name === "") {
      greeting.textContent = "Please enter a name first.";
    } else {
      greeting.textContent = "Hello, " + name + "!";
    }
  });
}
