// Native C++ application logic - the compiled-not-interpreted counterpart
// to the JavaScript wired up in main.cpp via JsEngine. Both this file and
// that script drive the exact same Node tree, through the exact same
// public Node API; nothing here is special-cased or requires the DOM to
// be aware of "native" handlers as a distinct concept. SetOnClick just
// stores a std::function<void()> - a plain C++ lambda closing over Node
// pointers works exactly as well as JsCallback's wrapped JS function
// does.

#include "app.h"

#include <iostream>

namespace artisan {

void SetupApp(Node &document) {
  Node *nameInput = document.FindById("name-input");
  Node *emailInput = document.FindById("email-input");
  Node *clearButton = document.FindById("clear-button");
  Node *greeting = document.FindById("greeting");

  if (nameInput == nullptr || emailInput == nullptr ||
      clearButton == nullptr || greeting == nullptr) {
    std::cerr << "SetupApp: this page is missing an expected id - "
                 "the Clear button won't be wired up\n";
    return;
  }

  clearButton->SetOnClick([nameInput, emailInput, greeting]() {
    nameInput->SetAttribute("value", "");
    emailInput->SetAttribute("value", "");
    greeting->SetTextContent("Fill in your name and click Submit.");
  });
}

} // namespace artisan
