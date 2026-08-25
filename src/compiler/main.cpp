// artisanc - compiles one or more HTML-like UI markup files (a "bundle")
// into a single C++ translation unit that defines artisan::CompiledPages(),
// a registry of {name, build-function} pairs - one per input file, named
// after its stem ("about.html" becomes "about"). Each build function
// constructs a mutable artisan::Node tree at startup: a structural mirror
// of that file's parsed markup (tag names, attributes, text, nesting),
// baked into the binary at build time so the final artisan executable
// never links lexbor and never parses markup at runtime. <img> file bytes
// are embedded as static byte arrays too, so images need no runtime file
// I/O either. main.cpp's Navigate() looks a page up by name when an
// <a href="..."> is clicked, to switch which one is live.
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
  // Never visible under any interpretation and never read by anything -
  // <script> content in particular is never used inline (a project's JS
  // comes from a separate file, see ARTISAN_APP_JS_SOURCE/embed_text),
  // so it's fine for artisanc to drop it rather than deferring to
  // BuildWidgetTree. <style> is NOT here despite never becoming a widget
  // itself (widget_tree_builder.cpp's own IsSkippedTag still keeps it out
  // of the Widget tree) - its text content is real, load-bearing CSS
  // (see css.h) that BuildWidgetTree reads at runtime, so it has to
  // survive into the compiled Node tree like any other element's text.
  return tag == "head" || tag == "script" || tag == "title" ||
         tag == "meta" || tag == "link";
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

// Accumulates what the generated file needs: top-level declarations (just
// <img> byte arrays, shared across every page so array names stay unique
// file-wide) in `decls`; every page's finished BuildPage_<name>() function
// body, one after another, in `pageFunctions`; `body` is where the page
// currently being compiled is written before getting appended there.
// `nodeCounter` resets per page (its names are function-local); `baseDir`
// is that page's own input file's directory, for resolving its <img> src
// paths.
struct CodegenContext {
  std::ostringstream decls;
  std::ostringstream body;
  std::ostringstream pageFunctions;
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

// The page name a <a href="..."> in one file navigates to another with -
// the file's stem, directory and extension stripped: "assets/about.html"
// becomes "about". Two input files that stem to the same name is a build
// error (checked in main), same as two C++ files defining the same symbol.
std::string StemName(const std::string &path) {
  size_t slash = path.find_last_of('/');
  std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
  size_t dot = base.find_last_of('.');
  return dot == std::string::npos ? base : base.substr(0, dot);
}

// Each input argument is either a bare path (its page name is derived
// from the stem above, e.g. by CMakeLists.txt's own default) or
// "name=path" with an explicit name - the form artisan-cli's project-dir
// discovery uses for a nested route a bare stem can't express, like
// "settings/profile" for pages/settings/profile.html. `name` is empty in
// the bare-path case, signaling the caller to fall back to StemName.
struct InputArg {
  std::string name;
  std::string path;
};

InputArg ParseInputArg(const std::string &arg) {
  size_t eq = arg.find('=');
  if (eq == std::string::npos) {
    return {"", arg};
  }
  return {arg.substr(0, eq), arg.substr(eq + 1)};
}

// A page name is arbitrary user-facing text (whatever the file was
// called); the C++ function built from it needs to be a valid identifier
// and unique even if two names only differ in characters this mangles
// away - "counter" -> "BuildPage_counter", anything not [A-Za-z0-9_]
// becomes '_', and a leading digit gets an underscore prefix since C++
// identifiers can't start with one.
std::string SanitizeIdentifier(const std::string &name) {
  std::string out = name;
  for (char &c : out) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
      c = '_';
    }
  }
  if (!out.empty() && std::isdigit(static_cast<unsigned char>(out.front()))) {
    out = "_" + out;
  }
  return out.empty() ? "_" : out;
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "usage: artisanc <output.cpp> <input1.html> "
                 "[<input2.html> ...]\n"
                 "       (an input may be \"name=path\" to give the page an\n"
                 "       explicit name instead of deriving one from path's\n"
                 "       stem - needed for a nested route like "
                 "\"settings/profile\")\n";
    return 1;
  }

  const std::string outputPath = argv[1];
  std::vector<InputArg> inputArgs;
  for (int i = 2; i < argc; ++i) {
    inputArgs.push_back(ParseInputArg(argv[i]));
  }

  CodegenContext ctx;

  struct Page {
    std::string name;
    std::string identifier;
  };
  std::vector<Page> pages;

  for (const InputArg &inputArg : inputArgs) {
    const std::string &inputPath = inputArg.path;
    artisan::HtmlDocument document(ReadFile(inputPath));

    lxb_dom_node_t *body = document.Body();
    if (body == nullptr) {
      std::cerr << "artisanc: " << inputPath << " has no <body>\n";
      return 1;
    }

    std::string name = inputArg.name.empty() ? StemName(inputPath) : inputArg.name;
    for (const Page &existing : pages) {
      if (existing.name == name) {
        std::cerr << "artisanc: two input files both name the page \""
                   << name << "\" - " << inputPath << " and an earlier one\n";
        return 1;
      }
    }
    std::string identifier = SanitizeIdentifier(name);

    ctx.baseDir = DirName(inputPath);
    ctx.body.str("");
    ctx.body.clear();
    ctx.nodeCounter = 0;

    ctx.body << "std::unique_ptr<Node> BuildPage_" << identifier << "() {\n";
    ctx.body << "  auto root = Node::CreateElement(\"body\");\n";
    EmitChildren(body, ctx, "root");
    ctx.body << "  return root;\n";
    ctx.body << "}\n\n";

    ctx.pageFunctions << ctx.body.str();
    pages.push_back({name, identifier});
  }

  std::ofstream out(outputPath);
  if (!out) {
    std::cerr << "artisanc: failed to open " << outputPath
               << " for writing\n";
    return 1;
  }

  out << "// Generated by artisanc from " << inputArgs.size()
      << " page(s) - do not edit.\n\n";
  out << "#include \"compiled_document.h\"\n";
  out << "#include \"dom_node.h\"\n\n";
  out << "#include <memory>\n";
  out << "#include <utility>\n";
  out << "#include <vector>\n\n";
  out << "namespace {\n\n";
  out << ctx.decls.str();
  out << "} // namespace\n\n";
  out << "namespace artisan {\n\n";
  out << "namespace {\n\n";
  out << ctx.pageFunctions.str();
  out << "} // namespace\n\n";
  out << "const std::vector<PageDescriptor> &CompiledPages() {\n";
  out << "  static const std::vector<PageDescriptor> kPages = {\n";
  for (const Page &page : pages) {
    out << "    {\"" << EscapeStringLiteral(page.name) << "\", &BuildPage_"
        << page.identifier << "},\n";
  }
  out << "  };\n";
  out << "  return kPages;\n";
  out << "}\n\n";
  out << "} // namespace artisan\n";

  return 0;
}
