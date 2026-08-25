// artisanc - compiles an HTML-like UI markup file into a C++ translation
// unit that defines artisan::BuildCompiledDocument(), a function that
// constructs a mutable artisan::Node tree at startup: a structural mirror
// of the parsed markup (tag names, attributes, text, nesting), baked into
// the binary at build time so the final artisan executable never links
// lexbor and never parses markup at runtime. <img> file bytes are
// embedded as static byte arrays too, so images need no runtime file I/O
// either.
//
// artisanc does no layout or rendering interpretation whatsoever - it
// doesn't know a <div> is block-level, or that colspan means anything, or
// what an <input>'s value even is for. That all happens exactly once,
// uniformly, in widget_tree_builder.cpp's BuildWidgetTree(), regardless
// of whether the Node tree it's handed came from this compiled path or
// was built by hand or by a script at runtime. That's what unifies the
// two: there is exactly one place that turns "a Node tree" into "a
// paintable Widget tree" - this file just produces Node trees, the same
// as any other code that calls Node::CreateElement.

#include "html_document.h"

#include <lexbor/html/html.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool IsSkippedTag(const std::string &tag) {
  // Never visible under any interpretation, and <script>/<style> content
  // in particular could be arbitrarily large text worth not even
  // emitting - unlike layout semantics (block/inline, table grids, ...),
  // this is a simple "don't bother" filter, not a rendering decision, so
  // it's fine for artisanc itself to make it rather than deferring to
  // BuildWidgetTree.
  return tag == "head" || tag == "script" || tag == "style" ||
         tag == "title" || tag == "meta" || tag == "link";
}

bool IsAllWhitespace(const std::string &text) {
  return std::all_of(text.begin(), text.end(),
                      [](unsigned char c) { return std::isspace(c); });
}

std::string EscapeStringLiteral(const std::string &text) {
  std::string out;
  out.reserve(text.size());

  for (char c : text) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\t':
      out += "\\t";
      break;
    case '\r':
      break;
    default:
      out += c;
    }
  }

  return out;
}

std::string TagName(lxb_dom_node_t *node) {
  lxb_dom_element_t *element = lxb_dom_interface_element(node);
  size_t len = 0;
  const lxb_char_t *name = lxb_dom_element_local_name(element, &len);
  return std::string(reinterpret_cast<const char *>(name), len);
}

std::string ResolveAssetPath(const std::string &baseDir,
                              const std::string &src) {
  if (baseDir.empty() || src.empty() || src.front() == '/') {
    return src;
  }
  return baseDir + "/" + src;
}

std::vector<unsigned char> ReadBinaryFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    return {};
  }

  std::streamsize size = file.tellg();
  if (size <= 0) {
    return {};
  }
  file.seekg(0, std::ios::beg);

  std::vector<unsigned char> buffer(static_cast<size_t>(size));
  if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
    return {};
  }

  return buffer;
}

// Accumulates the two things a generated file needs: top-level
// declarations (just <img> byte arrays) emitted before
// BuildCompiledDocument(), and the statements that make up its body -
// plus counters for unique local variable / array names.
struct CodegenContext {
  std::ostringstream decls;
  std::ostringstream body;
  int nodeCounter = 0;
  int imageCounter = 0;
  std::string baseDir;
};

std::string NewNodeVar(CodegenContext &ctx) {
  return "n" + std::to_string(ctx.nodeCounter++);
}

// Embeds `bytes` as a static byte array and emits the call wiring it to
// `nodeVar` (an <img> element) via Node::SetImageData.
void EmitImageBytes(CodegenContext &ctx, const std::string &nodeVar,
                     const std::vector<unsigned char> &bytes) {
  std::string arrayName = "kImageData" + std::to_string(ctx.imageCounter++);

  ctx.decls << "static const unsigned char " << arrayName << "[] = {\n";
  ctx.decls << std::hex << std::setfill('0');
  for (size_t i = 0; i < bytes.size(); ++i) {
    ctx.decls << (i % 20 == 0 ? "    " : "") << "0x" << std::setw(2)
               << static_cast<int>(bytes[i]) << ","
               << (i % 20 == 19 ? "\n" : " ");
  }
  // Reset the shared stream back to decimal in case anything else ever
  // writes plain numbers into ctx.decls.
  ctx.decls << std::dec << std::setfill(' ') << "\n};\n\n";

  ctx.body << "  " << nodeVar << "->SetImageData(" << arrayName << ", "
           << bytes.size() << ");\n";
}

void EmitChildren(lxb_dom_node_t *parent, CodegenContext &ctx,
                   const std::string &parentVar);

// Emits `auto nN = Node::CreateElement("tag"); ... nN->SetAttribute(...);
// ...children...; parentVar->AppendChild(std::move(nN));` for a single
// element node - a direct, meaning-free mirror of the DOM element: every
// attribute is copied verbatim as a string, regardless of what tag it's
// on or what that attribute means. <img>'s `src` is the one exception,
// used (not copied as-is) to find and embed the actual file bytes.
void EmitElement(lxb_dom_node_t *node, CodegenContext &ctx,
                  const std::string &parentVar) {
  std::string tag = TagName(node);
  std::string var = NewNodeVar(ctx);

  ctx.body << "  auto " << var << " = Node::CreateElement(\"" << tag
           << "\");\n";

  lxb_dom_element_t *element = lxb_dom_interface_element(node);
  std::string srcAttr;
  bool hasSrc = false;

  for (lxb_dom_attr_t *attr = lxb_dom_element_first_attribute(element);
       attr != nullptr; attr = lxb_dom_element_next_attribute(attr)) {
    size_t nameLen = 0;
    const lxb_char_t *namePtr = lxb_dom_attr_local_name(attr, &nameLen);
    std::string attrName(reinterpret_cast<const char *>(namePtr), nameLen);

    size_t valueLen = 0;
    const lxb_char_t *valuePtr = lxb_dom_attr_value(attr, &valueLen);
    std::string attrValue =
        valuePtr != nullptr
            ? std::string(reinterpret_cast<const char *>(valuePtr), valueLen)
            : std::string();

    ctx.body << "  " << var << "->SetAttribute(\""
              << EscapeStringLiteral(attrName) << "\", \""
              << EscapeStringLiteral(attrValue) << "\");\n";

    if (tag == "img" && attrName == "src") {
      srcAttr = attrValue;
      hasSrc = true;
    }
  }

  if (tag == "img" && hasSrc) {
    std::string path = ResolveAssetPath(ctx.baseDir, srcAttr);
    std::vector<unsigned char> bytes = ReadBinaryFile(path);
    if (bytes.empty()) {
      std::cerr << "artisanc: failed to read image \"" << path
                 << "\" - skipping\n";
    } else {
      EmitImageBytes(ctx, var, bytes);
    }
  }

  EmitChildren(node, ctx, var);

  ctx.body << "  " << parentVar << "->AppendChild(std::move(" << var
            << "));\n";
}

void EmitChildren(lxb_dom_node_t *parent, CodegenContext &ctx,
                   const std::string &parentVar) {
  for (lxb_dom_node_t *node = parent->first_child; node != nullptr;
       node = node->next) {
    if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
      std::string tag = TagName(node);
      if (IsSkippedTag(tag)) {
        continue;
      }
      EmitElement(node, ctx, parentVar);
    } else if (node->type == LXB_DOM_NODE_TYPE_TEXT) {
      size_t len = 0;
      const lxb_char_t *raw = lxb_dom_node_text_content(node, &len);
      if (raw == nullptr || len == 0) {
        continue;
      }

      std::string text(reinterpret_cast<const char *>(raw), len);
      if (IsAllWhitespace(text)) {
        continue;
      }

      std::string var = NewNodeVar(ctx);
      ctx.body << "  auto " << var << " = Node::CreateText(\""
                << EscapeStringLiteral(text) << "\");\n";
      ctx.body << "  " << parentVar << "->AppendChild(std::move(" << var
                << "));\n";
    }
  }
}

std::string ReadFile(const std::string &path) {
  std::ifstream file(path);
  if (!file) {
    std::cerr << "artisanc: failed to open " << path << "\n";
    std::exit(1);
  }

  std::ostringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

std::string DirName(const std::string &path) {
  size_t pos = path.find_last_of('/');
  if (pos == std::string::npos) {
    return "";
  }
  return path.substr(0, pos);
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "usage: artisanc <input.html> <output.cpp>\n";
    return 1;
  }

  const std::string inputPath = argv[1];
  const std::string outputPath = argv[2];

  artisan::HtmlDocument document(ReadFile(inputPath));

  lxb_dom_node_t *body = document.Body();
  if (body == nullptr) {
    std::cerr << "artisanc: " << inputPath << " has no <body>\n";
    return 1;
  }

  CodegenContext ctx;
  ctx.baseDir = DirName(inputPath);

  ctx.body << "std::unique_ptr<Node> BuildCompiledDocument() {\n";
  ctx.body << "  auto root = Node::CreateElement(\"body\");\n";
  EmitChildren(body, ctx, "root");
  ctx.body << "  return root;\n";
  ctx.body << "}\n";

  std::ofstream out(outputPath);
  if (!out) {
    std::cerr << "artisanc: failed to open " << outputPath
               << " for writing\n";
    return 1;
  }

  out << "// Generated by artisanc from " << inputPath << " - do not edit.\n\n";
  out << "#include \"dom_node.h\"\n\n";
  out << "#include <memory>\n";
  out << "#include <utility>\n\n";
  out << "namespace {\n\n";
  out << ctx.decls.str();
  out << "} // namespace\n\n";
  out << "namespace artisan {\n\n";
  out << ctx.body.str();
  out << "\n} // namespace artisan\n";

  return 0;
}
