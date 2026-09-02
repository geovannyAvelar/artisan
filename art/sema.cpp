#include "sema.h"

#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace ART {

namespace {
// Bare identifiers that are pure call-site sugar for a zero-arg function
// call, e.g. `document` for `ArtDocument()` - see CheckExpr's Identifier
// case for how this is actually applied (rewriting the Expr in place) and
// its own doc comment for the precedence rule (a real local/global always
// shadows this). Every entry's backing function still has to be declared
// by the project itself (this table doesn't declare anything on its
// own) - unlike a true builtin (e.g. numberToString), `ArtDocument` isn't
// self-contained: it depends on the project's own `Node`/`declare
// function ArtDocument` and the artisan runtime's C++ symbol behind it.
// `window` isn't included here yet - nothing in the DOM bridge is
// window-level (no timers/alerts/location/... exposed to ART yet), so
// there's nothing to desugar it to.
const std::unordered_map<std::string, std::string> kAmbientGlobals = {
    {"document", "ArtDocument"},
};
}

void Sema::Error(SourceLoc loc, const std::string &message) {
  std::ostringstream oss;
  oss << loc.line << ":" << loc.col << ": " << message;
  diagnostics.push_back(oss.str());
}

ResolvedType Sema::ResolveType(TypeNode *node) {
  switch (node->kind) {
  case TypeSyntaxKind::Number:
    return ResolvedType::Number();
  case TypeSyntaxKind::Boolean:
    return ResolvedType::Boolean();
  case TypeSyntaxKind::String:
    return ResolvedType::String();
  case TypeSyntaxKind::Handler: {
    std::vector<ResolvedType> params;
    params.reserve(node->handlerParamTypes.size());
    for (auto &p : node->handlerParamTypes) params.push_back(ResolveType(p.get()));
    return ResolvedType::Handler(std::move(params));
  }
  case TypeSyntaxKind::Void:
    return ResolvedType::Void();
  case TypeSyntaxKind::Array:
    return ResolvedType::ArrayOf(ResolveType(node->element.get()));
  case TypeSyntaxKind::Named: {
    // A generic instantiation's active substitution wins over everything
    // else: `T` inside a generic function's own signature/body always
    // means whatever concrete type this particular call site substituted
    // for it, never an interface named "T" (that would shadow the type
    // parameter, same as a real generic language's scoping rules). `T`
    // is always used bare (never as `T<...>` - ART has no higher-kinded
    // type parameters), so genericArgs is irrelevant here.
    if (currentSubstitution && node->genericArgs.empty()) {
      auto found = currentSubstitution->find(node->name);
      if (found != currentSubstitution->end()) return found->second;
    }

    if (!node->genericArgs.empty()) {
      auto it = genericInterfaces.find(node->name);
      bool foundAndVisible = it != genericInterfaces.end() && IsVisible(node->name);
      if (!foundAndVisible) {
        bool isPlainInterfaceVisible = interfaces.count(node->name) && IsVisible(node->name);
        if (isPlainInterfaceVisible) {
          Error(node->loc, "interface '" + node->name + "' is not generic - remove the type arguments");
        } else {
          Error(node->loc, "unknown generic type '" + node->name + "'" + VisibilityHint(node->name));
        }
        return ResolvedType{};
      }
      std::vector<ResolvedType> typeArgs;
      typeArgs.reserve(node->genericArgs.size());
      for (auto &a : node->genericArgs) typeArgs.push_back(ResolveType(a.get()));
      return InstantiateInterface(it->second, typeArgs, node->loc);
    }

    if (genericInterfaces.count(node->name) && IsVisible(node->name)) {
      Error(node->loc, "generic type '" + node->name + "' requires type arguments, e.g. '" + node->name + "<Type>'");
      return ResolvedType{};
    }
    if (interfaces.count(node->name) && IsVisible(node->name)) return ResolvedType::Struct(node->name);
    Error(node->loc, "unknown type '" + node->name + "'" + VisibilityHint(node->name));
    return ResolvedType{};
  }
  }
  return ResolvedType{};
}

ResolvedType Sema::InstantiateInterface(InterfaceDecl *tmpl, const std::vector<ResolvedType> &typeArgs,
                                         SourceLoc loc) {
  if (typeArgs.size() != tmpl->typeParams.size()) {
    Error(loc, "type '" + tmpl->name + "' expects " + std::to_string(tmpl->typeParams.size()) +
                   " type argument(s), got " + std::to_string(typeArgs.size()));
    return ResolvedType{};
  }

  std::string mangled = MangleInstantiation(tmpl->name, typeArgs);
  if (interfaces.count(mangled)) return ResolvedType::Struct(mangled);

  std::unordered_map<std::string, ResolvedType> subst;
  for (size_t i = 0; i < tmpl->typeParams.size(); i++) subst[tmpl->typeParams[i]] = typeArgs[i];

  auto clone = std::make_unique<InterfaceDecl>();
  clone->name = mangled;
  clone->loc = tmpl->loc;
  clone->isOpaque = tmpl->isOpaque;
  clone->sourceFile = tmpl->sourceFile;

  // Registered before its fields are resolved (not after) so a self-
  // referential generic interface resolves the recursive reference to
  // this same entry instead of instantiating forever - see the doc
  // comment on the declaration in sema.h.
  InterfaceDecl *inst = clone.get();
  interfaces[mangled] = inst;
  interfaceInstantiationStorage.push_back(std::move(clone));

  const std::unordered_map<std::string, ResolvedType> *savedSubst = currentSubstitution;
  currentSubstitution = &subst;
  // A field's type resolves lexically, against wherever the template
  // itself was declared - not wherever this particular instantiation was
  // asked for - same reasoning CheckGenericCall's own save/restore has.
  std::string savedFile = currentFile;
  currentFile = tmpl->sourceFile;
  for (auto &f : tmpl->fields) {
    InterfaceField cf;
    cf.name = f.name;
    cf.loc = f.loc;
    cf.resolvedType = ResolveType(f.type.get());
    inst->fields.push_back(std::move(cf));
  }

  // Class methods/accessors (see InterfaceDecl::methods) - cloned and
  // checked fresh per instantiation, the exact same "never checked in
  // template form" deal a generic function's own body has (see
  // CheckGenericCall): each clone gets its own AST since Sema mutates
  // resolvedType in place and two instantiations (e.g. `Box<number>` and
  // `Box<string>`) must not share nodes. Qualified against `mangled`
  // (this instantiation's own name, e.g. "Box$number"), not `tmpl->name`
  // ("Box") - see Check()'s class-registration pass for why the template
  // itself is never qualified at all. Get/set pairing and namespace-
  // clash validation already ran once, on the template, there - nothing
  // about those checks is type-dependent, so there's nothing to redo per
  // instantiation. A method's `this` parameter, injected by the parser
  // as `ClassName<T1, T2, ...>` for a generic class (see
  // Parser::InjectImplicitThis), resolves back to this exact
  // instantiation via the `interfaces[mangled] = inst` registration
  // above - safe even for a self-referential generic class for the same
  // reason fields already are.
  // Two phases, exactly mirroring how Check() already handles a non-
  // generic class's methods (register every signature first via
  // RegisterFunctionSignature, then check every body via
  // CheckFunctionBody) - a method must be able to call a sibling method
  // declared *later* in the same class body (e.g. `get value()` calling
  // `this.subscribe(...)`, defined further down - see "Signals" in
  // README.md), so every method's mere existence in inst->methods has to
  // be visible before any of their bodies are checked. A single
  // interleaved pass (clone-then-immediately-check, one method at a
  // time) would only ever see methods textually *before* the one
  // currently being checked.
  std::vector<FunctionDecl *> instMethods;
  instMethods.reserve(tmpl->methods.size());
  for (auto &m : tmpl->methods) {
    auto methodClone = std::make_unique<FunctionDecl>();
    methodClone->name = m->isGetter    ? MangleGetter(mangled, m->name)
                         : m->isSetter ? MangleSetter(mangled, m->name)
                                       : MangleMethod(mangled, m->name);
    methodClone->loc = m->loc;
    methodClone->sourceFile = tmpl->sourceFile;
    methodClone->isGetter = m->isGetter;
    methodClone->isSetter = m->isSetter;

    methodClone->params.reserve(m->params.size());
    for (auto &p : m->params) {
      Param cp;
      cp.name = p.name;
      cp.loc = p.loc;
      cp.resolvedType = ResolveType(p.type.get());
      methodClone->params.push_back(std::move(cp));
    }
    methodClone->resolvedReturnType = ResolveType(m->returnType.get());

    FunctionDecl *instMethod = methodClone.get();
    inst->methods.push_back(std::move(methodClone));
    instMethods.push_back(instMethod);
  }

  for (size_t idx = 0; idx < tmpl->methods.size(); idx++) {
    FunctionDecl *m = tmpl->methods[idx].get();
    FunctionDecl *instMethod = instMethods[idx];
    if (m->body) {
      instMethod->body = CloneStmt(*m->body);
      FunctionDecl *savedCurrentFunction = currentFunction;
      currentFunction = instMethod;
      PushScope();
      for (auto &p : instMethod->params) Declare(p.loc, p.name, p.resolvedType, /*isConst=*/false);
      CheckStmt(instMethod->body.get());
      PopScope();
      if (instMethod->resolvedReturnType.tag != TypeTag::Void && !AlwaysReturns(instMethod->body.get())) {
        Error(m->loc, "method '" + m->name + "' does not return a value of type " +
                          instMethod->resolvedReturnType.ToString() + " on all code paths (instantiated as '" +
                          instMethod->name + "')");
      }
      currentFunction = savedCurrentFunction;
    }
  }

  currentFile = savedFile;
  currentSubstitution = savedSubst;

  return ResolvedType::Struct(mangled);
}

std::string Sema::MangleType(const ResolvedType &t) {
  switch (t.tag) {
  case TypeTag::Number: return "number";
  case TypeTag::Boolean: return "boolean";
  case TypeTag::String: return "string";
  case TypeTag::Void: return "void";
  case TypeTag::Array: return "arr_" + MangleType(*t.elementType);
  case TypeTag::Struct: return t.structName;
  case TypeTag::Handler: {
    std::string out = "fn";
    for (auto &p : *t.handlerParamTypes) out += "_" + MangleType(p);
    return out;
  }
  case TypeTag::Unknown: return "unknown";
  }
  return "?";
}

std::string Sema::MangleInstantiation(const std::string &name, const std::vector<ResolvedType> &typeArgs) {
  std::string out = name;
  for (auto &arg : typeArgs) out += "$" + MangleType(arg);
  return out;
}

// See these four's own doc comments in sema.h. A "$get$"/"$set$" infix
// keeps a getter and setter sharing the same property name from
// colliding once qualified into the flat `functions` map both plain
// methods and accessors are registered into - see Check()'s own
// class-registration pass.
std::string Sema::MangleGetter(const std::string &className, const std::string &propName) {
  return className + "$get$" + propName;
}
std::string Sema::MangleSetter(const std::string &className, const std::string &propName) {
  return className + "$set$" + propName;
}
std::string Sema::MangleMethod(const std::string &className, const std::string &propName) {
  return className + "$" + propName;
}

const InterfaceField *Sema::FindField(const InterfaceDecl *iface, const std::string &name) {
  for (auto &f : iface->fields)
    if (f.name == name) return &f;
  return nullptr;
}
FunctionDecl *Sema::FindGetter(InterfaceDecl *iface, const std::string &name) {
  std::string mangled = MangleGetter(iface->name, name);
  for (auto &m : iface->methods)
    if (m->name == mangled) return m.get();
  return nullptr;
}
FunctionDecl *Sema::FindSetter(InterfaceDecl *iface, const std::string &name) {
  std::string mangled = MangleSetter(iface->name, name);
  for (auto &m : iface->methods)
    if (m->name == mangled) return m.get();
  return nullptr;
}
FunctionDecl *Sema::FindPlainMethod(InterfaceDecl *iface, const std::string &name) {
  std::string mangled = MangleMethod(iface->name, name);
  for (auto &m : iface->methods)
    if (m->name == mangled) return m.get();
  return nullptr;
}

void Sema::PushScope() { scopes.emplace_back(); }
void Sema::PopScope() { scopes.pop_back(); }

Sema::VarInfo *Sema::Lookup(const std::string &name) {
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) return &found->second;
  }
  auto globalIt = globals.find(name);
  if (globalIt != globals.end() && IsVisible(name)) return &globalIt->second;
  return nullptr;
}

void Sema::Declare(SourceLoc loc, const std::string &name, ResolvedType type, bool isConst) {
  auto &scope = scopes.back();
  if (scope.count(name)) {
    Error(loc, "'" + name + "' is already declared in this scope");
    return;
  }
  scope[name] = VarInfo{std::move(type), isConst};
}

bool Sema::IsVisible(const std::string &name) const {
  if (!visibility) return true; // no module graph resolved - fully flat/global, the original behavior
  if (builtinNames.count(name)) return true;
  auto it = visibility->find(currentFile);
  return it != visibility->end() && it->second.count(name) != 0;
}

std::string Sema::VisibilityHint(const std::string &name) const {
  if (!visibility) return "";
  bool existsAnywhere = globals.count(name) || functions.count(name) || interfaces.count(name) ||
                        genericFunctions.count(name) || genericInterfaces.count(name);
  return existsAnywhere ? " (it exists, but isn't imported into this file)" : "";
}

// ---------------------------------------------------------------------
// Top level
// ---------------------------------------------------------------------

void Sema::SeedBuiltins() {
  auto numberToString = std::make_unique<FunctionDecl>();
  numberToString->name = "numberToString";
  Param n;
  n.name = "n";
  n.resolvedType = ResolvedType::Number();
  numberToString->params.push_back(std::move(n));
  numberToString->resolvedReturnType = ResolvedType::String();
  // No body - Codegen::GenBuiltinNumberToString generates its LLVM
  // definition directly, the same way it does for every other function
  // signature it finds here, just without a Program::functions entry to
  // walk a Stmt body from.
  functions["numberToString"] = numberToString.get();
  builtinNames.insert("numberToString");
  builtins.push_back(std::move(numberToString));

  auto stringToNumber = std::make_unique<FunctionDecl>();
  stringToNumber->name = "stringToNumber";
  Param s;
  s.name = "s";
  s.resolvedType = ResolvedType::String();
  stringToNumber->params.push_back(std::move(s));
  stringToNumber->resolvedReturnType = ResolvedType::Number();
  // No body - Codegen::GenBuiltinStringToNumber generates its LLVM
  // definition directly, same as numberToString above.
  functions["stringToNumber"] = stringToNumber.get();
  builtinNames.insert("stringToNumber");
  builtins.push_back(std::move(stringToNumber));

  // makeArray<T>(size: number, fill: T): T[] - allocates a real,
  // *runtime-sized* array (every array literal, `[a, b, c]`, still has
  // to spell out every element at compile time - this is the one way to
  // build one whose length is only known at runtime). Registered as a
  // generic *template*, not a concrete function - like numberToString
  // above it has no ART-level body (there'd be nothing to build one
  // FROM: allocating a runtime-sized array is exactly the one thing
  // nothing already in the language can do), but unlike numberToString
  // it needs one real, hand-generated LLVM definition *per distinct T
  // actually used* (see Codegen::GenBuiltinMakeArray), the same
  // "template, no body, resolved and generated per instantiation" shape
  // a generic `declare function` already has (see CheckGenericCall's own
  // handling of a null `tmpl->body`) - `fill`'s param type is written as
  // a bare `T` TypeNode specifically so normal generic substitution
  // resolves it correctly per call site, the same way any other generic
  // function's own params would.
  auto makeArray = std::make_unique<FunctionDecl>();
  makeArray->name = "makeArray";
  makeArray->typeParams.push_back("T");

  Param sizeParam;
  sizeParam.name = "size";
  auto sizeType = std::make_unique<TypeNode>();
  sizeType->kind = TypeSyntaxKind::Number;
  sizeParam.type = std::move(sizeType);
  makeArray->params.push_back(std::move(sizeParam));

  Param fillParam;
  fillParam.name = "fill";
  auto fillType = std::make_unique<TypeNode>();
  fillType->kind = TypeSyntaxKind::Named;
  fillType->name = "T";
  fillParam.type = std::move(fillType);
  makeArray->params.push_back(std::move(fillParam));

  auto retElemType = std::make_unique<TypeNode>();
  retElemType->kind = TypeSyntaxKind::Named;
  retElemType->name = "T";
  auto retType = std::make_unique<TypeNode>();
  retType->kind = TypeSyntaxKind::Array;
  retType->element = std::move(retElemType);
  makeArray->returnType = std::move(retType);

  genericFunctions["makeArray"] = makeArray.get();
  builtinNames.insert("makeArray");
  builtins.push_back(std::move(makeArray));
}

bool Sema::Check(Program &program,
                  const std::unordered_map<std::string, std::unordered_set<std::string>> *visibilityMap) {
  visibility = visibilityMap;
  SeedBuiltins();

  for (auto &iface : program.interfaces) {
    bool alreadyDeclared = interfaces.count(iface->name) || genericInterfaces.count(iface->name);
    if (alreadyDeclared) {
      Error(iface->loc, "'" + iface->name + "' is already declared");
    }

    std::unordered_set<std::string> seenFields;
    for (auto &field : iface->fields) {
      if (!seenFields.insert(field.name).second) {
        Error(field.loc, "duplicate field '" + field.name + "' in interface '" + iface->name + "'");
      }
    }

    if (!iface->typeParams.empty()) {
      // A generic template's own field TypeNodes reference its type
      // parameters by name, same as a generic function's params do (see
      // RegisterFunctionSignature) - resolving them here, with no
      // substitution active yet, isn't needed for anything about the
      // template's own shape and would either misresolve a type
      // parameter as unknown or wrongly shadow a same-named interface.
      // InstantiateInterface resolves the (shared, read-only) TypeNodes
      // fresh, with substitution active, per actual instantiation instead.
      std::unordered_set<std::string> seenTypeParams;
      for (auto &t : iface->typeParams) {
        if (!seenTypeParams.insert(t).second) {
          Error(iface->loc, "duplicate type parameter '" + t + "' in interface '" + iface->name + "'");
        }
      }
      if (!alreadyDeclared) genericInterfaces[iface->name] = iface.get();
      continue;
    }

    if (!alreadyDeclared) interfaces[iface->name] = iface.get();
    currentFile = iface->sourceFile;
    for (auto &field : iface->fields) field.resolvedType = ResolveType(field.type.get());
  }
  currentFile.clear();

  // Every class's methods/accessors are qualified ("ClassName$methodName"/
  // "ClassName$get$propName"/"ClassName$set$propName" - see
  // MangleMethod/MangleGetter/MangleSetter) and handed off to the exact
  // same registration/body-checking machinery as any other top-level
  // function below - see InterfaceDecl::methods' own doc comment for why
  // a method/accessor is just call-site sugar over a plain, qualified
  // function rather than a distinct codegen concept. Done only once
  // every class/interface name above is registered, since a method's own
  // signature/body may reference any of them (including its own class,
  // or one declared later in the same file).
  //
  // A class's field/getter/setter/plain-method namespace is shared -
  // `obj.name` can only ever mean one thing - so a getter and a setter
  // may share a name (together they form one read/write property, the
  // only legal kind of duplicate here) but nothing else may collide with
  // an already-used name: two getters, two setters, two plain methods, a
  // getter/setter alongside a plain method, or any of those alongside an
  // already-declared field. This name-shape validation runs even for a
  // generic class template (it's purely about which names exist and
  // what kind each is, nothing to do with the type parameters actually
  // substituted later) - but a generic template's own methods are never
  // qualified/registered here at all, only per actual instantiation (see
  // InstantiateInterface), the same "never checked in template form"
  // deal a generic function's own body has (see the loops below) -
  // `iface->name` alone wouldn't even be a safe qualification prefix for
  // a template anyway, since two different instantiations (e.g.
  // `Box<number>`/`Box<string>`) need two distinctly-qualified copies of
  // each method, not one shared between them.
  for (auto &iface : program.interfaces) {
    std::unordered_set<std::string> fieldNames;
    for (auto &f : iface->fields) fieldNames.insert(f.name);
    std::unordered_set<std::string> seenGetters, seenSetters, seenPlainMethods;

    for (auto &method : iface->methods) {
      const std::string &propName = method->name; // still unqualified here
      bool clashesWithField = fieldNames.count(propName) != 0;
      if (method->isGetter) {
        if (clashesWithField) {
          Error(method->loc, "'" + propName + "' is already a field of '" + iface->name + "' - it can't also be a getter");
        } else if (seenPlainMethods.count(propName)) {
          Error(method->loc, "'" + propName + "' is already a method of '" + iface->name + "' - it can't also be a getter");
        } else if (!seenGetters.insert(propName).second) {
          Error(method->loc, "duplicate getter '" + propName + "' in class '" + iface->name + "'");
        }
      } else if (method->isSetter) {
        if (clashesWithField) {
          Error(method->loc, "'" + propName + "' is already a field of '" + iface->name + "' - it can't also be a setter");
        } else if (seenPlainMethods.count(propName)) {
          Error(method->loc, "'" + propName + "' is already a method of '" + iface->name + "' - it can't also be a setter");
        } else if (!seenSetters.insert(propName).second) {
          Error(method->loc, "duplicate setter '" + propName + "' in class '" + iface->name + "'");
        }
      } else {
        if (clashesWithField) {
          Error(method->loc, "'" + propName + "' is already a field of '" + iface->name + "' - it can't also be a method");
        } else if (seenGetters.count(propName) || seenSetters.count(propName)) {
          Error(method->loc, "'" + propName + "' is already a property (get/set) of '" + iface->name +
                                  "' - it can't also be a method");
        } else if (!seenPlainMethods.insert(propName).second) {
          Error(method->loc, "duplicate method '" + propName + "' in class '" + iface->name + "'");
        }
      }
      if (iface->typeParams.empty()) {
        method->sourceFile = iface->sourceFile;
        method->name = method->isGetter    ? MangleGetter(iface->name, propName)
                        : method->isSetter ? MangleSetter(iface->name, propName)
                                            : MangleMethod(iface->name, propName);
      }
    }
  }

  // Function signatures are registered before globals/top-level
  // statements so either's own diagnostics - e.g. a rejected call
  // expression's arguments - can still resolve a forward-declared
  // function instead of spuriously reporting it as undefined.
  for (auto &fn : program.functions) RegisterFunctionSignature(fn.get());
  for (auto &fn : program.externFunctions) RegisterFunctionSignature(fn.get());
  for (auto &iface : program.interfaces) {
    if (!iface->typeParams.empty()) continue; // a generic class - see InstantiateInterface instead
    for (auto &method : iface->methods) RegisterFunctionSignature(method.get());
  }

  // A project writing top-level statements (see Program::topLevelStmts'
  // own doc comment) instead of an explicit `function setupApp()` can't
  // also define one - both would try to produce the same generated
  // "setupApp" symbol, and there'd be no sensible way to decide which
  // one actually runs.
  if (!program.topLevelStmts.empty() && functions.count("setupApp")) {
    Error(program.topLevelStmts.front()->loc,
          "this project has both top-level statements and an explicit 'function setupApp()' - pick one: either "
          "remove the explicit function and let these statements become its body, or move this code inside it");
  }

  for (auto &g : program.globals) CheckGlobalDecl(g.get());

  // A generic function's body is never checked in template form - only
  // each concrete instantiation a call site actually asks for (see
  // CheckGenericCall), lazily, the same "never fully checked until used"
  // deal C++ templates have. A generic class's own methods have the
  // exact same deal (see InstantiateInterface).
  for (auto &fn : program.functions)
    if (fn->typeParams.empty()) CheckFunctionBody(fn.get());
  for (auto &iface : program.interfaces) {
    if (!iface->typeParams.empty()) continue;
    for (auto &method : iface->methods) CheckFunctionBody(method.get());
  }

  // Checked like any function body would be, just with no enclosing
  // FunctionDecl (currentFunction stays null - see CheckStmt's Return
  // case) and no scope of its own to push: each statement's own
  // identifiers resolve via the ordinary global lookup path (Lookup
  // falls through to the `globals` map when no local scope is active),
  // so a top-level statement referencing an earlier global "just works"
  // with no extra machinery.
  for (auto &s : program.topLevelStmts) {
    currentFile = s->sourceFile;
    CheckStmt(s.get());
  }
  currentFile.clear();

  return diagnostics.empty();
}

void Sema::CheckGlobalDecl(Stmt *stmt) {
  currentFile = stmt->sourceFile;
  bool hasDeclared = stmt->declaredType != nullptr;
  ResolvedType declared;
  if (hasDeclared) declared = ResolveType(stmt->declaredType.get());

  // A top-level `let`/`const` can be any type and any initializer
  // expression, checked exactly like a local variable's own - see
  // Codegen::GenGlobalInit for how a non-literal one (a call, an object/
  // array literal, another global, ...) actually gets computed: real
  // code, run once via a module constructor, in declaration order,
  // rather than a compile-time constant (which only a bare number/
  // boolean/string literal can still be - see GenGlobalDecl). A global's
  // own initializer may reference an *earlier* global (already stored by
  // then) but not a *later* one (still holds its type's zero value at
  // that point) - ordinary top-to-bottom declaration-order semantics,
  // the same "declare before use" rule most languages with static
  // initialization have, not specially enforced beyond that.
  ResolvedType actual = CheckExpr(stmt->expr.get(), hasDeclared ? &declared : nullptr);
  stmt->resolvedVarType = hasDeclared ? declared : actual;

  if (ExprUsesAmbientDocument(stmt->expr.get())) {
    Error(stmt->loc, "a top-level global's initializer can't use 'document' (directly) - a global is "
                      "initialized once, at process start, before any page has ever loaded, so 'document' is "
                      "always unusable there, not just in the narrow window ArtIsNull covers elsewhere. Wrap "
                      "this in a bare top-level block ('{ ... }') instead - see README.md's note on top-level "
                      "statements vs. globals");
  }

  if (!hasDeclared && actual.tag == TypeTag::Unknown) {
    Error(stmt->loc, "cannot infer a type for '" + stmt->varName + "' - add an explicit type annotation");
  } else if (!hasDeclared && actual.tag == TypeTag::Void) {
    Error(stmt->loc, "cannot declare '" + stmt->varName + "' with type void");
  }

  if (globals.count(stmt->varName) || functions.count(stmt->varName)) {
    Error(stmt->loc, "'" + stmt->varName + "' is already declared");
  } else {
    globals[stmt->varName] = VarInfo{stmt->resolvedVarType, stmt->isConst};
  }
}

void Sema::RegisterFunctionSignature(FunctionDecl *fn) {
  bool alreadyDeclared = functions.count(fn->name) || genericFunctions.count(fn->name);
  if (alreadyDeclared) {
    Error(fn->loc, "function '" + fn->name + "' is already declared");
  }

  std::unordered_set<std::string> seenParams;
  for (auto &p : fn->params) {
    if (!seenParams.insert(p.name).second) {
      Error(p.loc, "duplicate parameter '" + p.name + "' in function '" + fn->name + "'");
    }
  }

  if (!fn->typeParams.empty()) {
    // A generic template's own param/return TypeNodes reference its type
    // parameters (`T`, ...) by name, same syntax as any other named type -
    // resolving them here (with no substitution active yet) would either
    // wrongly treat a type parameter as an unknown type, or wrongly
    // shadow a same-named interface. Neither is needed anyway: nothing
    // about a template's own shape (arity is enough for a call site's
    // argument-count check) requires resolving these before an actual
    // instantiation substitutes concrete types - see CheckGenericCall,
    // which resolves the (shared, read-only) TypeNodes fresh per
    // instantiation instead.
    std::unordered_set<std::string> seenTypeParams;
    for (auto &t : fn->typeParams) {
      if (!seenTypeParams.insert(t).second) {
        Error(fn->loc, "duplicate type parameter '" + t + "' in function '" + fn->name + "'");
      }
    }
    if (!alreadyDeclared) genericFunctions[fn->name] = fn;
    return;
  }

  if (!alreadyDeclared) functions[fn->name] = fn;
  currentFile = fn->sourceFile;
  for (auto &p : fn->params) p.resolvedType = ResolveType(p.type.get());
  fn->resolvedReturnType = ResolveType(fn->returnType.get());
}

void Sema::CheckFunctionBody(FunctionDecl *decl) {
  currentFunction = decl;
  currentFile = decl->sourceFile;
  PushScope();
  for (auto &p : decl->params) Declare(p.loc, p.name, p.resolvedType, /*isConst=*/false);
  CheckStmt(decl->body.get());
  PopScope();
  if (decl->resolvedReturnType.tag != TypeTag::Void && !AlwaysReturns(decl->body.get())) {
    Error(decl->loc, "function '" + decl->name + "' does not return a value of type " +
                          decl->resolvedReturnType.ToString() + " on all code paths");
  }
  currentFunction = nullptr;
}

ResolvedType Sema::CheckGenericCall(Expr *expr) {
  const std::string &callee = expr->lhs->name;
  auto it = genericFunctions.find(callee);
  if (it == genericFunctions.end() || !IsVisible(callee)) {
    if (it == genericFunctions.end() && functions.count(callee) && IsVisible(callee)) {
      Error(expr->loc, "function '" + callee + "' is not generic - remove the type arguments");
    } else {
      Error(expr->loc, "call to undefined generic function '" + callee + "'" + VisibilityHint(callee));
    }
    for (auto &arg : expr->elements) CheckExpr(arg.get(), nullptr);
    expr->resolvedCalleeName = callee;
    return ResolvedType{};
  }
  FunctionDecl *tmpl = it->second;

  if (expr->typeArgs.size() != tmpl->typeParams.size()) {
    Error(expr->loc, "function '" + callee + "' expects " + std::to_string(tmpl->typeParams.size()) +
                          " type argument(s), got " + std::to_string(expr->typeArgs.size()));
    for (auto &arg : expr->elements) CheckExpr(arg.get(), nullptr);
    expr->resolvedCalleeName = callee;
    return ResolvedType{};
  }

  std::vector<ResolvedType> concreteArgs;
  concreteArgs.reserve(expr->typeArgs.size());
  for (auto &t : expr->typeArgs) concreteArgs.push_back(ResolveType(t.get()));

  std::string mangled = MangleInstantiation(callee, concreteArgs);
  expr->resolvedCalleeName = mangled;

  FunctionDecl *inst;
  auto instIt = instantiations.find(mangled);
  if (instIt != instantiations.end()) {
    inst = instIt->second;
  } else {
    std::unordered_map<std::string, ResolvedType> subst;
    for (size_t i = 0; i < tmpl->typeParams.size(); i++) subst[tmpl->typeParams[i]] = concreteArgs[i];

    auto clone = std::make_unique<FunctionDecl>();
    clone->name = mangled;
    clone->loc = tmpl->loc;
    clone->sourceFile = tmpl->sourceFile;

    const std::unordered_map<std::string, ResolvedType> *savedSubst = currentSubstitution;
    currentSubstitution = &subst;
    // Everything about this instantiation - its param/return types and
    // its body - resolves lexically, against wherever the template
    // itself was written, not wherever this particular call site is.
    std::string savedFile = currentFile;
    currentFile = tmpl->sourceFile;

    clone->params.reserve(tmpl->params.size());
    for (auto &p : tmpl->params) {
      Param cp;
      cp.name = p.name;
      cp.loc = p.loc;
      cp.resolvedType = ResolveType(p.type.get());
      clone->params.push_back(std::move(cp));
    }
    clone->resolvedReturnType = ResolveType(tmpl->returnType.get());

    // Registered before the body is checked (not after) so a direct
    // self-recursive call to this exact instantiation inside its own
    // body - found via CheckGenericCall re-entering with the same
    // mangled name - resolves against this (signature-complete, body
    // still in progress) entry instead of infinitely re-instantiating.
    inst = clone.get();
    instantiations[mangled] = inst;
    instantiationStorage.push_back(std::move(clone));

    if (tmpl->body) {
      inst->body = CloneStmt(*tmpl->body);
      FunctionDecl *savedCurrentFunction = currentFunction;
      currentFunction = inst;
      PushScope();
      for (auto &p : inst->params) Declare(p.loc, p.name, p.resolvedType, /*isConst=*/false);
      CheckStmt(inst->body.get());
      PopScope();
      if (inst->resolvedReturnType.tag != TypeTag::Void && !AlwaysReturns(inst->body.get())) {
        Error(tmpl->loc, "function '" + callee + "' does not return a value of type " +
                              inst->resolvedReturnType.ToString() + " on all code paths (instantiated as '" +
                              mangled + "')");
      }
      currentFunction = savedCurrentFunction;
    }
    // else: a generic `declare function` - no body to check; Codegen
    // treats a null body as extern, same as any non-generic one.

    currentFile = savedFile;
    currentSubstitution = savedSubst;
  }

  if (inst->params.size() != expr->elements.size()) {
    Error(expr->loc, "function '" + callee + "' expects " + std::to_string(inst->params.size()) +
                          " argument(s), got " + std::to_string(expr->elements.size()));
  }
  size_t n = std::min(inst->params.size(), expr->elements.size());
  for (size_t i = 0; i < n; i++) CheckExpr(expr->elements[i].get(), &inst->params[i].resolvedType);
  for (size_t i = n; i < expr->elements.size(); i++) CheckExpr(expr->elements[i].get(), nullptr);

  return inst->resolvedReturnType;
}

bool Sema::AlwaysReturns(Stmt *stmt) {
  switch (stmt->kind) {
  case StmtKind::Return:
    return true;
  case StmtKind::Block:
    for (auto &s : stmt->statements)
      if (AlwaysReturns(s.get())) return true;
    return false;
  case StmtKind::If:
    return stmt->elseBranch && AlwaysReturns(stmt->body.get()) && AlwaysReturns(stmt->elseBranch.get());
  default:
    return false;
  }
}

// See this method's own doc comment in sema.h.
bool Sema::ExprUsesAmbientDocument(const Expr *expr) const {
  if (!expr) return false;
  if (expr->kind == ExprKind::Call) {
    auto ambient = kAmbientGlobals.find("document");
    if (ambient != kAmbientGlobals.end() && expr->resolvedCalleeName == ambient->second) return true;
  }
  if (ExprUsesAmbientDocument(expr->lhs.get())) return true;
  if (ExprUsesAmbientDocument(expr->rhs.get())) return true;
  if (ExprUsesAmbientDocument(expr->operand.get())) return true;
  for (auto &e : expr->elements)
    if (ExprUsesAmbientDocument(e.get())) return true;
  for (auto &f : expr->fields)
    if (ExprUsesAmbientDocument(f.second.get())) return true;
  return false;
}

// ---------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------

void Sema::CheckStmt(Stmt *stmt) {
  switch (stmt->kind) {
  case StmtKind::VarDecl: {
    bool hasDeclared = stmt->declaredType != nullptr;
    ResolvedType declared;
    if (hasDeclared) declared = ResolveType(stmt->declaredType.get());
    ResolvedType actual = CheckExpr(stmt->expr.get(), hasDeclared ? &declared : nullptr);
    stmt->resolvedVarType = hasDeclared ? declared : actual;
    if (!hasDeclared && actual.tag == TypeTag::Unknown) {
      Error(stmt->loc, "cannot infer a type for '" + stmt->varName + "' - add an explicit type annotation");
    } else if (!hasDeclared && actual.tag == TypeTag::Void) {
      Error(stmt->loc, "cannot declare '" + stmt->varName + "' with type void");
    }
    Declare(stmt->loc, stmt->varName, stmt->resolvedVarType, stmt->isConst);
    break;
  }
  case StmtKind::If: {
    ResolvedType boolT = ResolvedType::Boolean();
    CheckExpr(stmt->cond.get(), &boolT);
    CheckStmt(stmt->body.get());
    if (stmt->elseBranch) CheckStmt(stmt->elseBranch.get());
    break;
  }
  case StmtKind::While: {
    ResolvedType boolT = ResolvedType::Boolean();
    CheckExpr(stmt->cond.get(), &boolT);
    CheckStmt(stmt->body.get());
    break;
  }
  case StmtKind::For: {
    PushScope(); // so a `let` in the initializer is scoped to just the loop
    if (stmt->initStmt) CheckStmt(stmt->initStmt.get());
    if (stmt->cond) {
      ResolvedType boolT = ResolvedType::Boolean();
      CheckExpr(stmt->cond.get(), &boolT);
    }
    if (stmt->update) CheckExpr(stmt->update.get(), nullptr);
    CheckStmt(stmt->body.get());
    PopScope();
    break;
  }
  case StmtKind::ForOf: {
    ResolvedType iterableT = CheckExpr(stmt->expr.get(), nullptr);
    ResolvedType elemT;
    if (iterableT.tag == TypeTag::Array) {
      elemT = *iterableT.elementType;
    } else if (iterableT.tag != TypeTag::Unknown) {
      Error(stmt->loc, "'for...of' requires an array, found '" + iterableT.ToString() + "'");
    }
    stmt->resolvedVarType = elemT;
    PushScope();
    Declare(stmt->loc, stmt->varName, elemT, stmt->isConst);
    CheckStmt(stmt->body.get());
    PopScope();
    break;
  }
  case StmtKind::Return: {
    if (!currentFunction) {
      // Only possible from a top-level statement (see
      // Program::topLevelStmts) - a function body always has
      // currentFunction set (see CheckFunctionBody/CheckGenericCall/
      // InstantiateInterface).
      Error(stmt->loc, "cannot 'return' outside a function");
      if (stmt->expr) CheckExpr(stmt->expr.get(), nullptr);
      break;
    }
    ResolvedType retT = currentFunction->resolvedReturnType;
    if (retT.tag == TypeTag::Void) {
      if (stmt->expr) {
        Error(stmt->loc, "function '" + currentFunction->name + "' returns void but this 'return' has a value");
        CheckExpr(stmt->expr.get(), nullptr);
      }
    } else if (!stmt->expr) {
      Error(stmt->loc, "function '" + currentFunction->name + "' must return a value of type " + retT.ToString());
    } else {
      CheckExpr(stmt->expr.get(), &retT);
    }
    break;
  }
  case StmtKind::ExprStmt:
    CheckExpr(stmt->expr.get(), nullptr);
    break;
  case StmtKind::Block:
    PushScope();
    for (auto &s : stmt->statements) CheckStmt(s.get());
    PopScope();
    break;
  }
}

// ---------------------------------------------------------------------
// Lvalues (shared by Assign and IncDec)
// ---------------------------------------------------------------------

ResolvedType Sema::CheckLValueTarget(Expr *target, SourceLoc opLoc) {
  if (target->kind == ExprKind::Identifier) {
    VarInfo *v = Lookup(target->name);
    ResolvedType t;
    if (!v) {
      Error(target->loc, "undefined identifier '" + target->name + "'" + VisibilityHint(target->name));
      t = ResolvedType{};
    } else {
      if (v->isConst) Error(opLoc, "cannot assign to '" + target->name + "' - it is declared 'const'");
      t = v->type;
    }
    target->resolvedType = t;
    return t;
  }

  if (target->kind == ExprKind::Index) {
    ResolvedType t = CheckExpr(target, nullptr);
    if (target->lhs->resolvedType.tag == TypeTag::String) {
      Error(opLoc, "strings are immutable - cannot assign to a character");
    }
    return t;
  }

  if (target->kind == ExprKind::Member) {
    // Deliberately not just CheckExpr(target, nullptr) - unlike a plain
    // read, an lvalue target can't let CheckExpr's own Member case
    // rewrite a getter-backed property into a Call in place (there's
    // nothing sensible to assign a call's result into), and a
    // setter-backed property needs a completely different resolution
    // (see FindSetter below) that read-only field/getter access never
    // needs at all.
    ResolvedType objT = CheckExpr(target->lhs.get(), nullptr);
    if (objT.tag == TypeTag::Array || objT.tag == TypeTag::String) {
      if (target->name == "length") {
        target->isLengthAccess = true;
        Error(opLoc, "cannot assign to '.length' - it is read-only");
      } else {
        Error(opLoc, "cannot access member '" + target->name + "' on type '" + objT.ToString() + "'");
      }
      target->resolvedType = ResolvedType::Number();
      return target->resolvedType;
    }
    if (objT.tag == TypeTag::Struct) {
      InterfaceDecl *iface = interfaces.at(objT.structName);
      if (const InterfaceField *field = FindField(iface, target->name)) {
        target->resolvedType = field->resolvedType;
        return field->resolvedType;
      }
      if (FunctionDecl *setter = FindSetter(iface, target->name)) {
        // Codegen's Assign case checks this to call the setter instead
        // of doing a plain field store - see Expr::resolvedCalleeName's
        // own doc comment. setter->params[1] is the value parameter
        // (params[0] is the implicit `this`).
        target->resolvedCalleeName = setter->name;
        target->resolvedType = setter->params[1].resolvedType;
        return target->resolvedType;
      }
      std::string hint;
      if (FindGetter(iface, target->name)) {
        hint = " - it's a read-only property (it has a getter but no setter)";
      } else if (FindPlainMethod(iface, target->name)) {
        hint = " - it's a method, and can only be used with call syntax ('(...)')";
      }
      Error(opLoc, "interface '" + iface->name + "' has no field '" + target->name + "'" + hint);
      target->resolvedType = ResolvedType{};
      return target->resolvedType;
    }
    if (objT.tag != TypeTag::Unknown) {
      Error(opLoc, "cannot access member '" + target->name + "' on type '" + objT.ToString() + "'");
    }
    target->resolvedType = ResolvedType{};
    return target->resolvedType;
  }

  Error(opLoc, "invalid assignment target");
  return ResolvedType{};
}

// ---------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------

ResolvedType Sema::CheckExpr(Expr *expr, const ResolvedType *expected) {
  ResolvedType actual;

  switch (expr->kind) {
  case ExprKind::NumberLiteral:
    actual = ResolvedType::Number();
    break;

  case ExprKind::BoolLiteral:
    actual = ResolvedType::Boolean();
    break;

  case ExprKind::StringLiteral:
    actual = ResolvedType::String();
    break;

  case ExprKind::Identifier: {
    VarInfo *v = Lookup(expr->name);
    auto funcIt = functions.find(expr->name);
    if (v) {
      actual = v->type;
    } else if (funcIt != functions.end() && IsVisible(expr->name)) {
      // A bare function name (not a call) - the only first-class value ART
      // functions have. Only a void-returning function can be one (ART has
      // no closures, so this is always just a plain code address, never a
      // captured environment) - its own parameter types become the
      // Handler's, checked structurally like any other type against
      // whatever "(params...) => void" the use site actually expects.
      FunctionDecl *fn = funcIt->second;
      if (fn->resolvedReturnType.tag != TypeTag::Void) {
        Error(expr->loc, "function '" + expr->name +
                              "' can't be used as a value - only a void-returning function can "
                              "(as a handler)");
        actual = ResolvedType{};
      } else {
        std::vector<ResolvedType> paramTypes;
        paramTypes.reserve(fn->params.size());
        for (auto &p : fn->params) paramTypes.push_back(p.resolvedType);
        actual = ResolvedType::Handler(std::move(paramTypes));
      }
    } else if (genericFunctions.count(expr->name) && IsVisible(expr->name)) {
      Error(expr->loc, "generic function '" + expr->name +
                            "' can't be used as a value - only a fully-instantiated call is supported "
                            "('" + expr->name + "::<Type>(...)')");
      actual = ResolvedType{};
    } else if (auto ambient = kAmbientGlobals.find(expr->name);
               ambient != kAmbientGlobals.end() && functions.count(ambient->second) &&
               IsVisible(ambient->second)) {
      // `document` (see kAmbientGlobals) isn't a real variable or a
      // declared function of its own - it's pure sugar for a call to its
      // backing zero-arg function (ArtDocument), rewritten in place right
      // here so Codegen never needs to know this identifier is anything
      // special: an ExprKind::Call with no arguments is exactly what a
      // literal `ArtDocument()` in source would already produce. Only
      // fires once every other, more specific meaning of the bare name
      // (a real local/global variable, an ordinary or generic function
      // used as a value) has already been ruled out above - a project
      // that genuinely declares its own `document` shadows this sugar,
      // same precedence a real local always has over anything ambient.
      FunctionDecl *backing = functions.at(ambient->second);
      if (!backing->params.empty()) {
        Error(expr->loc, "'" + ambient->second + "' backs the ambient '" + expr->name +
                              "' and must take no parameters");
        actual = ResolvedType{};
      } else {
        expr->kind = ExprKind::Call;
        expr->resolvedCalleeName = ambient->second;
        actual = backing->resolvedReturnType;
      }
    } else {
      Error(expr->loc, "undefined identifier '" + expr->name + "'" + VisibilityHint(expr->name));
      actual = ResolvedType{};
    }
    break;
  }

  case ExprKind::Binary: {
    ResolvedType lhsT = CheckExpr(expr->lhs.get(), nullptr);
    ResolvedType rhsT = CheckExpr(expr->rhs.get(), nullptr);
    const std::string &op = expr->op;
    bool lhsUnknown = lhsT.tag == TypeTag::Unknown;
    bool rhsUnknown = rhsT.tag == TypeTag::Unknown;
    if (op == "+" && (lhsT.tag == TypeTag::String || rhsT.tag == TypeTag::String)) {
      if (!lhsUnknown && !rhsUnknown && (lhsT.tag != TypeTag::String || rhsT.tag != TypeTag::String)) {
        Error(expr->loc, "operator '+' requires two strings or two numbers, found '" + lhsT.ToString() + "' and '" +
                              rhsT.ToString() + "'");
      }
      actual = ResolvedType::String();
    } else if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
      if (!lhsUnknown && !rhsUnknown && (lhsT.tag != TypeTag::Number || rhsT.tag != TypeTag::Number)) {
        Error(expr->loc, "operator '" + op + "' requires two numbers, found '" + lhsT.ToString() + "' and '" +
                              rhsT.ToString() + "'");
      }
      actual = ResolvedType::Number();
    } else if (op == "<" || op == "<=" || op == ">" || op == ">=") {
      if (!lhsUnknown && !rhsUnknown && (lhsT.tag != TypeTag::Number || rhsT.tag != TypeTag::Number)) {
        Error(expr->loc, "operator '" + op + "' requires two numbers, found '" + lhsT.ToString() + "' and '" +
                              rhsT.ToString() + "'");
      }
      actual = ResolvedType::Boolean();
    } else if (op == "==" || op == "!=") {
      if (!lhsUnknown && !rhsUnknown && lhsT != rhsT) {
        Error(expr->loc, "operator '" + op + "' requires operands of the same type, found '" + lhsT.ToString() +
                              "' and '" + rhsT.ToString() + "'");
      }
      actual = ResolvedType::Boolean();
    } else if (op == "&&" || op == "||") {
      if (!lhsUnknown && !rhsUnknown && (lhsT.tag != TypeTag::Boolean || rhsT.tag != TypeTag::Boolean)) {
        Error(expr->loc, "operator '" + op + "' requires two booleans, found '" + lhsT.ToString() + "' and '" +
                              rhsT.ToString() + "'");
      }
      actual = ResolvedType::Boolean();
    } else {
      Error(expr->loc, "unknown operator '" + op + "'");
      actual = ResolvedType{};
    }
    break;
  }

  case ExprKind::Unary: {
    ResolvedType operandT = CheckExpr(expr->operand.get(), nullptr);
    bool unknown = operandT.tag == TypeTag::Unknown;
    if (expr->op == "-") {
      if (!unknown && operandT.tag != TypeTag::Number) {
        Error(expr->loc, "unary '-' requires a number, found '" + operandT.ToString() + "'");
      }
      actual = ResolvedType::Number();
    } else if (expr->op == "!") {
      if (!unknown && operandT.tag != TypeTag::Boolean) {
        Error(expr->loc, "unary '!' requires a boolean, found '" + operandT.ToString() + "'");
      }
      actual = ResolvedType::Boolean();
    } else {
      Error(expr->loc, "unknown unary operator '" + expr->op + "'");
      actual = ResolvedType{};
    }
    break;
  }

  case ExprKind::Call: {
    if (expr->lhs->kind == ExprKind::Member) {
      // `obj.method(args)` - always resolved statically against obj's
      // declared type (no vtable/dynamic dispatch - see
      // InterfaceDecl::methods' own doc comment), so this is pure
      // call-site sugar for a plain call to the class's already-
      // qualified method function, with obj spliced in as the actual
      // first argument once every other argument has been checked.
      Expr *memberExpr = expr->lhs.get();
      ResolvedType objT = CheckExpr(memberExpr->lhs.get(), nullptr);
      if (!expr->typeArgs.empty()) {
        Error(expr->loc, "a method call can't take explicit type arguments yet");
      }
      if (objT.tag != TypeTag::Struct) {
        if (objT.tag != TypeTag::Unknown) {
          Error(expr->loc, "cannot call '." + memberExpr->name + "' on type '" + objT.ToString() +
                                "' - only a class's methods support call syntax");
        }
        for (auto &arg : expr->elements) CheckExpr(arg.get(), nullptr);
        actual = ResolvedType{};
        break;
      }
      InterfaceDecl *iface = interfaces.at(objT.structName);
      FunctionDecl *method = FindPlainMethod(iface, memberExpr->name);
      if (!method) {
        std::string hint;
        if (FindField(iface, memberExpr->name)) {
          hint = " (it's a field, not callable)";
        } else if (FindGetter(iface, memberExpr->name) || FindSetter(iface, memberExpr->name)) {
          hint = " (it's a property - access it without '()', e.g. '." + memberExpr->name + "')";
        }
        Error(expr->loc, "'" + iface->name + "' has no method '" + memberExpr->name + "'" + hint);
        for (auto &arg : expr->elements) CheckExpr(arg.get(), nullptr);
        actual = ResolvedType{};
        break;
      }
      expr->resolvedCalleeName = method->name;
      // method->params[0] is the implicit `this` receiver - not part of
      // the ART-visible argument list obj.method(args) supplies.
      size_t userParamCount = method->params.size() - 1;
      if (userParamCount != expr->elements.size()) {
        Error(expr->loc, "method '" + memberExpr->name + "' expects " + std::to_string(userParamCount) +
                              " argument(s), got " + std::to_string(expr->elements.size()));
      }
      size_t n = std::min(userParamCount, expr->elements.size());
      for (size_t i = 0; i < n; i++) CheckExpr(expr->elements[i].get(), &method->params[i + 1].resolvedType);
      for (size_t i = n; i < expr->elements.size(); i++) CheckExpr(expr->elements[i].get(), nullptr);
      expr->elements.insert(expr->elements.begin(), std::move(memberExpr->lhs));
      actual = method->resolvedReturnType;
      break;
    }
    if (!expr->typeArgs.empty()) {
      if (expr->lhs->kind != ExprKind::Identifier) {
        Error(expr->loc, "only a direct call to a named generic function can take explicit type arguments");
        for (auto &arg : expr->elements) CheckExpr(arg.get(), nullptr);
        actual = ResolvedType{};
        break;
      }
      actual = CheckGenericCall(expr);
      break;
    }

    if (expr->lhs->kind == ExprKind::Identifier && !Lookup(expr->lhs->name)) {
      // No local/global *variable* shadows this name - try it as an
      // ordinary named-function call first (unchanged from before this
      // callee could also be an indirect one - see below).
      const std::string &callee = expr->lhs->name;
      auto it = functions.find(callee);
      if (it != functions.end() && IsVisible(callee)) {
        expr->resolvedCalleeName = callee;
        FunctionDecl *fn = it->second;
        if (fn->params.size() != expr->elements.size()) {
          Error(expr->loc, "function '" + callee + "' expects " + std::to_string(fn->params.size()) +
                                " argument(s), got " + std::to_string(expr->elements.size()));
        }
        size_t n = std::min(fn->params.size(), expr->elements.size());
        for (size_t i = 0; i < n; i++) CheckExpr(expr->elements[i].get(), &fn->params[i].resolvedType);
        for (size_t i = n; i < expr->elements.size(); i++) CheckExpr(expr->elements[i].get(), nullptr);
        actual = fn->resolvedReturnType;
        break;
      }
      if (genericFunctions.count(callee) && IsVisible(callee)) {
        Error(expr->loc, "generic function '" + callee + "' requires explicit type arguments, e.g. '" + callee +
                              "::<Type>(...)'");
        for (auto &arg : expr->elements) CheckExpr(arg.get(), nullptr);
        actual = ResolvedType{};
        break;
      }
      // Falls through to the general Handler-value call path below - not
      // a known function name either, so it reports as an ordinary
      // undefined identifier from there if it isn't some other kind of
      // Handler-valued expression.
    }

    // General case: call any Handler-valued expression - a plain
    // variable, an array element, ... anything that isn't a class method
    // or a named top-level function (both handled above). Always an
    // *indirect* call at the LLVM level (see Expr::isIndirectCall and
    // Codegen's own Call case) - there's no symbol name to look up here,
    // just a computed function-pointer value. This is what actually lets
    // a reactive value's stored subscriber callbacks be invoked at all
    // (see "Signals" in README.md) - a bare function name alone could
    // never express "whichever handler happens to be in this slot".
    ResolvedType calleeT = CheckExpr(expr->lhs.get(), nullptr);
    if (calleeT.tag != TypeTag::Handler) {
      if (calleeT.tag != TypeTag::Unknown) {
        Error(expr->loc, "cannot call a value of type '" + calleeT.ToString() + "'");
      }
      for (auto &arg : expr->elements) CheckExpr(arg.get(), nullptr);
      actual = ResolvedType{};
      break;
    }
    const std::vector<ResolvedType> &paramTypes = *calleeT.handlerParamTypes;
    if (paramTypes.size() != expr->elements.size()) {
      Error(expr->loc, "expected " + std::to_string(paramTypes.size()) + " argument(s), got " +
                            std::to_string(expr->elements.size()));
    }
    size_t n = std::min(paramTypes.size(), expr->elements.size());
    for (size_t i = 0; i < n; i++) CheckExpr(expr->elements[i].get(), &paramTypes[i]);
    for (size_t i = n; i < expr->elements.size(); i++) CheckExpr(expr->elements[i].get(), nullptr);
    expr->isIndirectCall = true;
    actual = ResolvedType::Void();
    break;
  }

  case ExprKind::ArrayLiteral: {
    if (expected && expected->tag == TypeTag::Array) {
      for (auto &el : expr->elements) CheckExpr(el.get(), expected->elementType.get());
      actual = *expected;
    } else if (expr->elements.empty()) {
      Error(expr->loc, "cannot infer the element type of an empty array literal - add an explicit type annotation");
      actual = ResolvedType{};
    } else {
      ResolvedType elemT = CheckExpr(expr->elements[0].get(), nullptr);
      for (size_t i = 1; i < expr->elements.size(); i++) CheckExpr(expr->elements[i].get(), &elemT);
      actual = ResolvedType::ArrayOf(elemT);
    }
    break;
  }

  case ExprKind::ObjectLiteral: {
    if (!expected || expected->tag != TypeTag::Struct) {
      Error(expr->loc, "object literal needs a target interface type - add an explicit type annotation");
      for (auto &f : expr->fields) CheckExpr(f.second.get(), nullptr);
      actual = ResolvedType{};
      break;
    }
    InterfaceDecl *iface = interfaces.at(expected->structName);
    if (iface->isOpaque) {
      Error(expr->loc, "'" + iface->name +
                            "' is an opaque foreign type (declared with 'declare type') - it has no "
                            "accessible fields and can't be constructed with '{}', only obtained from "
                            "a 'declare function' call");
      for (auto &f : expr->fields) CheckExpr(f.second.get(), nullptr);
      actual = *expected;
      break;
    }
    std::unordered_set<std::string> provided;
    for (auto &f : expr->fields) {
      const InterfaceField *field = nullptr;
      for (auto &candidate : iface->fields)
        if (candidate.name == f.first) { field = &candidate; break; }
      if (!field) {
        Error(expr->loc, "interface '" + iface->name + "' has no field '" + f.first + "'");
        CheckExpr(f.second.get(), nullptr);
        continue;
      }
      if (!provided.insert(f.first).second) {
        Error(expr->loc, "duplicate field '" + f.first + "' in object literal");
      }
      CheckExpr(f.second.get(), &field->resolvedType);
    }
    for (auto &field : iface->fields) {
      if (!provided.count(field.name)) {
        Error(expr->loc, "missing field '" + field.name + "' required by interface '" + iface->name + "'");
      }
    }
    actual = *expected;
    break;
  }

  case ExprKind::Index: {
    ResolvedType objT = CheckExpr(expr->lhs.get(), nullptr);
    ResolvedType idxT = CheckExpr(expr->operand.get(), nullptr);
    if (objT.tag == TypeTag::Unknown) {
      actual = ResolvedType{};
    } else if (objT.tag != TypeTag::Array && objT.tag != TypeTag::String) {
      Error(expr->loc, "cannot index into type '" + objT.ToString() + "' - only arrays and strings support '[]'");
      actual = ResolvedType{};
    } else {
      if (idxT.tag != TypeTag::Unknown && idxT.tag != TypeTag::Number) {
        Error(expr->loc, "index must be a number, found '" + idxT.ToString() + "'");
      }
      // Indexing a string yields a 1-character string, same as real TS.
      actual = objT.tag == TypeTag::String ? ResolvedType::String() : *objT.elementType;
    }
    break;
  }

  case ExprKind::Member: {
    ResolvedType objT = CheckExpr(expr->lhs.get(), nullptr);
    if ((objT.tag == TypeTag::Array || objT.tag == TypeTag::String) && expr->name == "length") {
      expr->isLengthAccess = true;
      actual = ResolvedType::Number();
    } else if (objT.tag == TypeTag::Struct) {
      InterfaceDecl *iface = interfaces.at(objT.structName);
      const InterfaceField *field = FindField(iface, expr->name);
      if (field) {
        actual = field->resolvedType;
      } else if (FunctionDecl *getter = FindGetter(iface, expr->name)) {
        // `obj.prop` where `prop` is a `get` accessor - pure call-site
        // sugar, rewritten in place into an ordinary zero-arg call with
        // the receiver spliced in as the actual first argument, the
        // exact same trick the ambient `document` identifier and a
        // plain method call already use (see CheckExpr's Call case and
        // Sema::kAmbientGlobals) - Codegen never needs to know this Call
        // started life as a Member at all.
        expr->kind = ExprKind::Call;
        expr->resolvedCalleeName = getter->name;
        expr->elements.insert(expr->elements.begin(), std::move(expr->lhs));
        actual = getter->resolvedReturnType;
      } else {
        std::string hint;
        if (FindPlainMethod(iface, expr->name)) {
          hint = " - it's a method, and can only be used with call syntax ('(...)')";
        } else if (FindSetter(iface, expr->name)) {
          hint = " - it's a write-only property (it has a setter but no getter)";
        }
        Error(expr->loc, "interface '" + iface->name + "' has no field '" + expr->name + "'" + hint);
        actual = ResolvedType{};
      }
    } else if (objT.tag == TypeTag::Unknown) {
      actual = ResolvedType{};
    } else {
      Error(expr->loc, "cannot access member '" + expr->name + "' on type '" + objT.ToString() + "'");
      actual = ResolvedType{};
    }
    break;
  }

  case ExprKind::IncDec: {
    ResolvedType targetT = CheckLValueTarget(expr->operand.get(), expr->loc);
    // A setter-backed property (unlike a plain field) has no real memory
    // address for GenLValue to increment in place - doing this properly
    // would mean reading through its getter, adding one, then writing
    // back through its setter, a distinct codegen path this doesn't
    // build (see CheckLValueTarget's own doc comment) - rejected here,
    // deliberately, rather than silently miscompiling.
    if (expr->operand->kind == ExprKind::Member && !expr->operand->resolvedCalleeName.empty()) {
      Error(expr->loc, "cannot use '" + expr->op + "' on '" + expr->operand->name +
                            "' - a setter-backed property doesn't support increment/decrement");
    }
    if (targetT.tag != TypeTag::Unknown && targetT.tag != TypeTag::Number) {
      Error(expr->loc, "operator '" + expr->op + "' requires a number, found '" + targetT.ToString() + "'");
    }
    actual = ResolvedType::Number();
    break;
  }

  case ExprKind::Assign: {
    ResolvedType targetT = CheckLValueTarget(expr->lhs.get(), expr->loc);
    CheckExpr(expr->rhs.get(), &targetT);
    actual = targetT;
    break;
  }
  }

  expr->resolvedType = actual;
  if (expected && actual.tag != TypeTag::Unknown && expected->tag != TypeTag::Unknown && actual != *expected) {
    Error(expr->loc, "type mismatch: expected '" + expected->ToString() + "', found '" + actual.ToString() + "'");
  }
  return actual;
}

} // namespace ART
