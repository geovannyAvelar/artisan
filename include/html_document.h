#pragma once

#include <string>

struct lxb_html_document;
struct lxb_dom_node;

namespace artisan {

// Owns a parsed HTML document (lexbor's html5 parser under the hood) and
// exposes just the DOM tree a layout engine needs to walk.
class HtmlDocument {
public:
  explicit HtmlDocument(const std::string &html);
  ~HtmlDocument();

  HtmlDocument(const HtmlDocument &) = delete;
  HtmlDocument &operator=(const HtmlDocument &) = delete;

  bool IsValid() const { return document_ != nullptr; }

  // The <body> node, or nullptr if parsing failed or there is no body.
  struct lxb_dom_node *Body() const;

private:
  struct lxb_html_document *document_ = nullptr;
};

} // namespace artisan
