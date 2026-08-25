#include "html_document.h"

#include <lexbor/html/html.h>

#include <iostream>

namespace artisan {

HtmlDocument::HtmlDocument(const std::string &html) {
  lxb_html_document_t *document = lxb_html_document_create();
  if (document == nullptr) {
    std::cerr << "Failed to create lexbor document\n";
    return;
  }

  lxb_status_t status = lxb_html_document_parse(
      document, reinterpret_cast<const lxb_char_t *>(html.c_str()),
      html.size());

  if (status != LXB_STATUS_OK) {
    std::cerr << "lexbor failed to parse HTML\n";
    lxb_html_document_destroy(document);
    return;
  }

  document_ = document;
}

HtmlDocument::~HtmlDocument() {
  if (document_ != nullptr) {
    lxb_html_document_destroy(document_);
  }
}

lxb_dom_node *HtmlDocument::Body() const {
  if (document_ == nullptr) {
    return nullptr;
  }

  return lxb_dom_interface_node(lxb_html_document_body_element(document_));
}

} // namespace artisan
