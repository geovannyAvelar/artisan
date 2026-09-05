#include "sema.h"

#include <functional>
#include <iterator>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace ART {

// kAmbientGlobals now lives in ast.h - Parser needs it too (see its own
// doc comment there).

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
  case TypeSyntaxKind::Any:
    return ResolvedType::Any();
  case TypeSyntaxKind::Array:
    return ResolvedType::ArrayOf(ResolveType(node->element.get()));
  case TypeSyntaxKind::Nullable: {
    ResolvedType inner = ResolveType(node->element.get());
    if (inner.tag == TypeTag::Nullable) {
      Error(node->loc, "'T | null | null' isn't meaningful - a type can only be nullable once");
      return inner; // degrade gracefully - already nullable, nothing more to wrap
    }
    if (inner.tag == TypeTag::Any) {
      Error(node->loc, "'any | null' is redundant - 'any' already includes 'null', just write 'any'");
      return inner; // degrade gracefully - Any absorbs null already
    }
    return ResolvedType::NullableOf(std::move(inner));
  }
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
    if (enums.count(node->name) && IsVisible(node->name)) return ResolvedType::Enum(node->name);
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
      frameStack.push_back(instMethod);
      PushScope();
      for (auto &p : instMethod->params) Declare(p.loc, p.name, p.resolvedType, /*isConst=*/false, &p);
      int savedLoopDepth = loopDepth, savedSwitchDepth = switchDepth;
      loopDepth = 0;
      switchDepth = 0;
      CheckStmt(instMethod->body.get());
      loopDepth = savedLoopDepth;
      switchDepth = savedSwitchDepth;
      PopScope();
      frameStack.pop_back();
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
  case TypeTag::Nullable: return "opt_" + MangleType(*t.elementType);
  case TypeTag::Any: return "any";
  case TypeTag::Struct: return t.structName;
  case TypeTag::Enum: return t.structName;
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
std::string Sema::MangleStatic(const std::string &className, const std::string &propName) {
  return className + "$static$" + propName;
}
std::string Sema::MangleEnumMember(const std::string &enumName, const std::string &memberName) {
  return enumName + "$enum$" + memberName;
}

bool Sema::IsStaticAccessTarget(const std::string &name) {
  return (interfaces.count(name) != 0 || enums.count(name) != 0) && !Lookup(name);
}

const InterfaceField *Sema::FindField(const InterfaceDecl *iface, const std::string &name) {
  for (auto &f : iface->fields)
    if (f.name == name) return &f;
  return nullptr;
}
FunctionDecl *Sema::FindGetter(InterfaceDecl *iface, const std::string &name) {
  // Walks up the inheritance chain (a no-op single iteration for a class
  // with no baseClass, i.e. every class before inheritance existed at
  // all) - unlike FindField, this can't just rely on a flattened list:
  // an ancestor's own method is mangled under ITS OWN name
  // ("Animal$get$prop", not "Dog$get$prop"), so each level has to be
  // checked with its own class's own mangling.
  for (InterfaceDecl *cur = iface; cur; cur = cur->baseClass) {
    std::string mangled = MangleGetter(cur->name, name);
    for (auto &m : cur->methods)
      if (m->name == mangled) return m.get();
  }
  return nullptr;
}
FunctionDecl *Sema::FindSetter(InterfaceDecl *iface, const std::string &name) {
  for (InterfaceDecl *cur = iface; cur; cur = cur->baseClass) {
    std::string mangled = MangleSetter(cur->name, name);
    for (auto &m : cur->methods)
      if (m->name == mangled) return m.get();
  }
  return nullptr;
}
FunctionDecl *Sema::FindPlainMethod(InterfaceDecl *iface, const std::string &name) {
  for (InterfaceDecl *cur = iface; cur; cur = cur->baseClass) {
    std::string mangled = MangleMethod(cur->name, name);
    for (auto &m : cur->methods)
      if (m->name == mangled) return m.get();
  }
  return nullptr;
}

void Sema::PushScope() {
  scopes.emplace_back();
  scopeFrameDepth.push_back(frameStack.size());
}
void Sema::PopScope() {
  scopes.pop_back();
  scopeFrameDepth.pop_back();
}

Sema::VarInfo *Sema::Lookup(const std::string &name) {
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    auto found = it->find(name);
    if (found == it->end()) continue;
    size_t idx = static_cast<size_t>(std::distance(scopes.begin(), it.base()) - 1);
    size_t declaredAtDepth = scopeFrameDepth[idx];
    if (declaredAtDepth < frameStack.size()) {
      // A real lexical capture: `name` was declared in an ancestor
      // frame, not just an outer block within the SAME frame currently
      // being checked. Mark both ends - see this method's own doc
      // comment in sema.h.
      VarInfo &v = found->second;
      if (v.declParam) v.declParam->isCapturedByClosure = true;
      if (v.declStmt) v.declStmt->isCapturedByClosure = true;
      for (size_t k = declaredAtDepth; k < frameStack.size(); k++) {
        FunctionDecl *frame = frameStack[k];
        bool already = false;
        for (auto &c : frame->captures)
          if (c.name == name) { already = true; break; }
        if (!already) frame->captures.push_back({name, v.type});
      }
    }
    return &found->second;
  }
  auto globalIt = globals.find(name);
  if (globalIt != globals.end() && IsVisible(name)) return &globalIt->second;
  return nullptr;
}

void Sema::Declare(SourceLoc loc, const std::string &name, ResolvedType type, bool isConst, Param *declParam,
                    Stmt *declStmt) {
  auto &scope = scopes.back();
  if (scope.count(name)) {
    Error(loc, "'" + name + "' is already declared in this scope");
    return;
  }
  scope[name] = VarInfo{std::move(type), isConst, declParam, declStmt};
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
                        genericFunctions.count(name) || genericInterfaces.count(name) || enums.count(name);
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

  // notNull<T>(v: T): T | null - the ONLY way to produce a genuinely
  // present `T | null` value (besides passing along one you already
  // have) - there's no implicit "a plain T widens to T | null"
  // anywhere in ART, matching the language's usual "explicit, not
  // implicit" stance (numberToString instead of implicit
  // stringification, makeArray<T> instead of a magic runtime-sized
  // literal, ...). Same "generic template, no ART-level body, one real
  // hand-generated LLVM definition per distinct T" shape makeArray<T>
  // already has (see Codegen::GenBuiltinNotNull) - boxes `v` into a
  // fresh GC cell and returns that pointer as the Nullable(T) value
  // (see TypeTag::Nullable's own doc comment for why every T | null is
  // uniformly boxed like this, never just T's own bit pattern).
  auto notNull = std::make_unique<FunctionDecl>();
  notNull->name = "notNull";
  notNull->typeParams.push_back("T");

  Param notNullParam;
  notNullParam.name = "v";
  auto notNullParamType = std::make_unique<TypeNode>();
  notNullParamType->kind = TypeSyntaxKind::Named;
  notNullParamType->name = "T";
  notNullParam.type = std::move(notNullParamType);
  notNull->params.push_back(std::move(notNullParam));

  auto notNullRetInner = std::make_unique<TypeNode>();
  notNullRetInner->kind = TypeSyntaxKind::Named;
  notNullRetInner->name = "T";
  auto notNullRetType = std::make_unique<TypeNode>();
  notNullRetType->kind = TypeSyntaxKind::Nullable;
  notNullRetType->element = std::move(notNullRetInner);
  notNull->returnType = std::move(notNullRetType);

  genericFunctions["notNull"] = notNull.get();
  builtinNames.insert("notNull");
  builtins.push_back(std::move(notNull));

  // `Error` - the one throwable/catchable type `throw`/`catch` recognize
  // right now (see StmtKind::Try's own doc comment for why just one).
  // An ordinary struct in every other respect - `{ message: string }` -
  // constructed the normal way (`{ message: "..." }`), with no special
  // Codegen support needed for its own layout at all (GetOrCreateStructType
  // builds its LLVM type the exact same generic way any other interface's
  // already is - only StmtKind::Try/Throw's own codegen, and the setjmp/
  // longjmp machinery underneath them, are new).
  auto errorIface = std::make_unique<InterfaceDecl>();
  errorIface->name = "Error";
  InterfaceField messageField;
  messageField.name = "message";
  messageField.resolvedType = ResolvedType::String();
  errorIface->fields.push_back(std::move(messageField));
  interfaces["Error"] = errorIface.get();
  builtinNames.insert("Error");
  builtinInterfaces.push_back(std::move(errorIface));
}

bool Sema::Check(Program &program,
                  const std::unordered_map<std::string, std::unordered_set<std::string>> *visibilityMap) {
  visibility = visibilityMap;
  SeedBuiltins();

  // Every static field (InterfaceDecl::staticFields) and enum member
  // (EnumDecl::members), across the whole program, desugars into a real
  // synthetic global - hoisted out here as it's found and spliced onto
  // the FRONT of program.globals below (before the program's own
  // top-level globals), so each is checked/codegen'd by the exact same
  // machinery an ordinary global already has, with no bespoke Codegen
  // path needed for either. Declared in program-declaration order (every
  // class/enum, interleaved exactly as source order already has them),
  // then member-declaration order within each - so any top-level global
  // can reference an earlier class's static field or an earlier enum's
  // member, and a static field/enum member's own initializer can
  // reference an earlier one of either kind, but never a later-declared
  // plain top-level global (ordinary "declare before use", same as
  // globals already have among themselves).
  std::vector<std::unique_ptr<Stmt>> hoistedGlobals;

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

  // `extends` resolution: link each class's own baseClassName to a real
  // InterfaceDecl*, validate it, and flatten its fields (base's own,
  // recursively, then this class's new ones) directly into `fields` -
  // done this way (a real, physical copy, not a separate "walk the
  // chain" lookup elsewhere) so every OTHER piece of code that already
  // reads `iface->fields` - object-literal construction requiring every
  // field, FindField, Codegen's own GetOrCreateStructType/FieldIndex -
  // needs zero changes at all to correctly see inherited fields too.
  // Methods are NOT flattened this way (see FindPlainMethod/FindGetter/
  // FindSetter's own chain-walking below instead) - unlike a field's
  // fixed compile-time layout, which class's own method implementation
  // a call reaches can depend on the instance's actual runtime type
  // (virtual dispatch), so "just copy the FunctionDecl pointer" would be
  // wrong for a family with more than one class actually overriding
  // something.
  {
    // Memoized (`resolved`), so a multi-level chain (Grandchild extends
    // Child extends Base) is flattened correctly regardless of which
    // order the three classes happen to appear in program.interfaces -
    // each is processed at most once, and processing a derived class
    // first still correctly processes its own base first internally (a
    // plain recursive call), not out of order. `visiting` tracks the
    // current recursion stack specifically to catch a genuine cycle (A
    // extends B extends A): checking only "is base == iface" would miss
    // one that closes more than one level up, since an intermediate
    // class's own baseClass pointer isn't filled in yet at the point a
    // naive same-level check would need it.
    // `broken` propagates UP the recursion, not just at the exact point
    // a cycle is detected: without it, an outer frame (e.g. A, waiting
    // on resolveInheritance(B), which itself detects the cycle back to
    // A) would still go ahead and set `iface->baseClass = base` using
    // that cyclic pointer - which would then infinite-loop the LATER
    // hasVirtualDispatch-marking and vtable-computation passes below,
    // neither of which has (or needs) its own cycle protection, since
    // they trust baseClass chains to be genuinely acyclic by the time
    // they run. Every class anywhere in a detected cycle (or that
    // extends, even transitively, into one) ends up in `broken` and
    // keeps baseClass null - no inheritance applied, past the one
    // reported error.
    std::unordered_set<InterfaceDecl *> resolved, visiting, broken;
    std::function<void(InterfaceDecl *)> resolveInheritance = [&](InterfaceDecl *iface) {
      if (resolved.count(iface)) return;
      if (iface->baseClassName.empty()) {
        resolved.insert(iface);
        return;
      }
      if (visiting.count(iface)) {
        Error(iface->loc, "circular inheritance involving '" + iface->name + "'");
        broken.insert(iface);
        resolved.insert(iface);
        return;
      }
      visiting.insert(iface);

      auto baseIt = interfaces.find(iface->baseClassName);
      if (baseIt == interfaces.end() || genericInterfaces.count(iface->baseClassName)) {
        Error(iface->loc, "class '" + iface->name + "' can't extend '" + iface->baseClassName +
                               "' - it isn't a plain, non-generic class");
        broken.insert(iface);
      } else {
        InterfaceDecl *base = baseIt->second;
        if (base->isOpaque) {
          Error(iface->loc, "class '" + iface->name + "' can't extend '" + iface->baseClassName +
                                 "' - a 'declare class' has no accessible fields to inherit");
          broken.insert(iface);
        } else {
          resolveInheritance(base); // base-before-derived - base's own fields must already be flattened
          if (broken.count(base)) {
            // `base` is itself part of a cycle (or extends into one) -
            // don't propagate its unreliable state into `iface` too.
            broken.insert(iface);
            visiting.erase(iface);
            resolved.insert(iface);
            return;
          }
          iface->baseClass = base;

          std::unordered_set<std::string> inheritedNames;
          for (auto &f : base->fields) inheritedNames.insert(f.name);
          for (auto &f : iface->fields) {
            if (inheritedNames.count(f.name)) {
              Error(f.loc, "'" + f.name + "' is already a field of '" + base->name + "' - '" + iface->name +
                               "' can't redeclare it (no field shadowing)");
            }
          }
          // Prepend - base's own fields (already fully flattened,
          // including whatever IT inherited) come first, so a Dog* is
          // layout-compatible with an Animal* (see
          // InterfaceDecl::baseClass's own doc comment on why that
          // ordering is what makes upcasting free). InterfaceField isn't
          // copyable (its own `type` is a unique_ptr, never read again
          // after the initial per-field ResolveType call above runs, so
          // there's nothing worth cloning it for here) - built field by
          // field instead of a vector copy, `type` left null on each
          // synthesized copy.
          std::vector<InterfaceField> merged;
          merged.reserve(base->fields.size() + iface->fields.size());
          for (auto &f : base->fields) {
            InterfaceField copy;
            copy.name = f.name;
            copy.resolvedType = f.resolvedType;
            copy.loc = f.loc;
            copy.isReadonly = f.isReadonly;
            merged.push_back(std::move(copy));
          }
          for (auto &f : iface->fields) merged.push_back(std::move(f));
          iface->fields = std::move(merged);
        }
      }
      visiting.erase(iface);
      resolved.insert(iface);
    };

    for (auto &iface : program.interfaces) {
      if (iface->typeParams.empty()) resolveInheritance(iface.get());
    }
  }

  // A class only gets real virtual dispatch (a vtable pointer in its own
  // instances, indirect calls for its methods - see
  // InterfaceDecl::hasVirtualDispatch's own doc comment) if it's
  // actually touched by *some* extends relationship - has a baseClass,
  // or is one for something else. Every ancestor of an extended class
  // needs the flag too (Animal itself needs a vtable pointer the moment
  // ANYTHING extends it, even though Animal's own declaration never
  // mentions `extends` at all) - walking up from each derived class
  // marks its whole chain.
  for (auto &iface : program.interfaces) {
    if (iface->baseClass) {
      for (InterfaceDecl *cur = iface.get(); cur; cur = cur->baseClass) cur->hasVirtualDispatch = true;
    }
  }

  // Each enum member desugars into a real global constant - see
  // EnumDecl's own doc comment and Sema::MangleEnumMember. Auto-numbered
  // from 0 (or from an explicit `= N`, continuing +1 from whichever
  // value - explicit or auto - the previous member ended up with),
  // exactly real TS's own numeric-enum rule.
  for (auto &decl : program.enums) {
    bool alreadyDeclared = interfaces.count(decl->name) || genericInterfaces.count(decl->name) ||
                            enums.count(decl->name) || functions.count(decl->name);
    if (alreadyDeclared) {
      Error(decl->loc, "'" + decl->name + "' is already declared");
    } else {
      enums[decl->name] = decl.get();
    }

    std::unordered_set<std::string> seenMembers;
    double nextValue = 0;
    for (auto &member : decl->members) {
      if (!seenMembers.insert(member.name).second) {
        Error(member.loc, "duplicate member '" + member.name + "' in enum '" + decl->name + "'");
      }
      member.value = member.hasExplicitValue ? member.explicitValue : nextValue;
      nextValue = member.value + 1;

      auto global = MakeStmt(StmtKind::VarDecl, member.loc);
      global->isConst = true;
      global->isPreCheckedGlobal = true; // see its own doc comment
      global->sourceFile = decl->sourceFile;
      global->varName = MangleEnumMember(decl->name, member.name);
      global->resolvedVarType = ResolvedType::Enum(decl->name);
      auto valueExpr = MakeExpr(ExprKind::NumberLiteral, member.loc);
      valueExpr->numberValue = member.value;
      valueExpr->resolvedType = ResolvedType::Enum(decl->name);
      global->expr = std::move(valueExpr);
      // Registered directly, right here (not deferred to the later
      // per-global CheckGlobalDecl loop, which this global skips
      // entirely - see isPreCheckedGlobal) so a LATER enum's own member,
      // or any other code checked before that loop runs, can already
      // see this one - same "declare before use, in this loop's own
      // order" deal a real global has.
      globals[global->varName] = VarInfo{global->resolvedVarType, /*isConst=*/true};
      hoistedGlobals.push_back(std::move(global));
    }
  }

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
    // A static field/method's own namespace - entirely separate from the
    // instance one above (fieldNames/seenGetters/seenSetters/
    // seenPlainMethods): `ClassName.foo` and `instance.foo` can share a
    // name with no collision at all, matching real TS/JS. Shared between
    // static methods (below) and static fields (the follow-up loop right
    // after this one), since both live in the one static namespace.
    std::unordered_set<std::string> seenStaticNames;

    for (auto &method : iface->methods) {
      const std::string &propName = method->name; // still unqualified here
      if (method->isStatic) {
        if (!seenStaticNames.insert(propName).second) {
          Error(method->loc, "duplicate static member '" + propName + "' in class '" + iface->name + "'");
        }
        if (iface->typeParams.empty()) {
          method->sourceFile = iface->sourceFile;
          method->name = MangleStatic(iface->name, propName);
        }
        continue;
      }
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

    for (auto &field : iface->staticFields) {
      const std::string &propName = field->varName; // still unqualified here
      if (!seenStaticNames.insert(propName).second) {
        Error(field->loc, "duplicate static member '" + propName + "' in class '" + iface->name + "'");
      }
      field->sourceFile = iface->sourceFile;
      field->varName = MangleStatic(iface->name, propName);
      hoistedGlobals.push_back(std::move(field));
    }
  }
  program.globals.insert(program.globals.begin(), std::make_move_iterator(hoistedGlobals.begin()),
                          std::make_move_iterator(hoistedGlobals.end()));

  // Function signatures are registered before globals/top-level
  // statements so either's own diagnostics - e.g. a rejected call
  // expression's arguments - can still resolve a forward-declared
  // function instead of spuriously reporting it as undefined.
  for (auto &fn : program.functions) RegisterFunctionSignature(fn.get(), /*allowRestParam=*/true);
  for (auto &fn : program.externFunctions) RegisterFunctionSignature(fn.get());
  for (auto &iface : program.interfaces) {
    if (!iface->typeParams.empty()) continue; // a generic class - see InstantiateInterface instead
    for (auto &method : iface->methods) RegisterFunctionSignature(method.get());
  }

  // Vtable slot assignment - only for a class with hasVirtualDispatch
  // (see its own doc comment); every other class's methods keep
  // isVirtual false / vtableSlot -1 (their defaults), meaning Codegen
  // generates a plain direct call for them exactly as it always has.
  // Only plain instance methods participate (not get/set accessors or
  // static methods - see FunctionDecl::isVirtual's own doc comment for
  // the accepted gap on accessors). Slots are numbered base-to-derived,
  // memoized per class so a multi-level chain is handled in the right
  // order regardless of declaration order, same shape the field-
  // flattening pass above already has.
  {
    struct VtableEntry {
      std::string name;    // the method's own unqualified name (shared across every override in the family)
      FunctionDecl *impl;  // whichever class's own version currently "wins" this slot
    };
    std::unordered_map<InterfaceDecl *, std::vector<VtableEntry>> layouts;
    std::function<void(InterfaceDecl *)> computeVtable = [&](InterfaceDecl *iface) {
      if (layouts.count(iface) || !iface->hasVirtualDispatch) return;
      std::vector<VtableEntry> layout;
      if (iface->baseClass) {
        computeVtable(iface->baseClass);
        layout = layouts[iface->baseClass]; // a real copy - each class's own slot numbers are independent of edits to this one
      }
      for (auto &m : iface->methods) {
        if (m->isGetter || m->isSetter || m->isStatic) continue;
        std::string unqualified = m->name.substr(iface->name.size() + 1); // strip "IfaceName$" - see MangleMethod
        VtableEntry *existing = nullptr;
        for (auto &entry : layout) {
          if (entry.name == unqualified) {
            existing = &entry;
            break;
          }
        }
        if (existing) {
          // An override - same slot as the ancestor's own version, but
          // only if the signature actually matches (params beyond the
          // implicit `this`, and the return type) - ART has no
          // covariance/contravariance rules to make good on a mismatched
          // one, so this is a hard requirement, not a warning.
          FunctionDecl *baseMethod = existing->impl;
          bool sigMatches = m->params.size() == baseMethod->params.size();
          for (size_t i = 1; sigMatches && i < m->params.size(); i++) {
            sigMatches = m->params[i].resolvedType == baseMethod->params[i].resolvedType;
          }
          sigMatches = sigMatches && m->resolvedReturnType == baseMethod->resolvedReturnType;
          if (!sigMatches) {
            Error(m->loc, "'" + unqualified + "' overrides a method of an ancestor class with a different "
                                               "signature - an override must match exactly (same parameter "
                                               "types, same return type)");
          }
          m->vtableSlot = baseMethod->vtableSlot;
          existing->impl = m.get();
        } else {
          m->vtableSlot = static_cast<int>(layout.size());
          layout.push_back({unqualified, m.get()});
        }
        m->isVirtual = true;
      }
      layouts[iface] = std::move(layout);
    };
    for (auto &iface : program.interfaces) {
      if (iface->typeParams.empty()) computeVtable(iface.get());
    }
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

  for (auto &g : program.globals) {
    if (!g->isPreCheckedGlobal) CheckGlobalDecl(g.get());
  }

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
  // case). One shared scope for the whole sequence, matching
  // Codegen::GenSetupAppBody's own PushScope/PopScope around its
  // identical loop - needed now that a bare top-level `let`/`const`
  // can land directly here (see Parser::ParseProgram: a document-
  // touching one is reclassified out of `globals` into here instead of
  // being rejected), since CheckStmt's VarDecl case declares into
  // `scopes.back()` unconditionally (Declare) - nothing to push into
  // without this. A statement referencing an *earlier* global still
  // "just works" the same way it always has: Lookup falls through to
  // the `globals` map only after this scope (and any it nests) comes
  // up empty.
  PushScope();
  for (auto &s : program.topLevelStmts) {
    currentFile = s->sourceFile;
    CheckStmt(s.get());
  }
  PopScope();
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

  // A plain (non-exported) top-level `let`/`const` whose initializer
  // uses `document` never reaches this check at all - Parser::
  // ParseProgram already reclassified it into a per-page-load local
  // instead (program.topLevelStmts, not program.globals - see its own
  // doc comment). What's left here is the one case that can't be
  // rescued that way: `export`ed. An export is a promise of a real,
  // cross-file-visible persistent global - initialized once, at
  // process start, before any page has ever loaded, so 'document' is
  // always unusable there, not just in the narrow window ArtIsNull
  // covers elsewhere - and a document-touching declaration is a
  // per-call local by nature, which can't be exported.
  if (ExprUsesAmbientDocument(stmt->expr.get())) {
    Error(stmt->loc, "an exported global's initializer can't use 'document' - exports are for real persistent "
                      "globals (initialized once, at process start, before any page has ever loaded), and a "
                      "'document'-touching declaration is a per-page-load local by nature, which can't be "
                      "exported. Remove 'export', or restructure so nothing needing 'document' has to be shared "
                      "across files - see README.md's note on top-level statements vs. globals");
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

void Sema::RegisterFunctionSignature(FunctionDecl *fn, bool allowRestParam) {
  bool alreadyDeclared = functions.count(fn->name) || genericFunctions.count(fn->name);
  if (alreadyDeclared) {
    Error(fn->loc, "function '" + fn->name + "' is already declared");
  }

  std::unordered_set<std::string> seenParams;
  for (auto &p : fn->params) {
    if (!seenParams.insert(p.name).second) {
      Error(p.loc, "duplicate parameter '" + p.name + "' in function '" + fn->name + "'");
    }
    // A rest parameter is only supported on a plain, non-generic,
    // top-level function in this first pass - not a class method, a
    // `declare function` (a native FFI boundary needs a fixed,
    // predictable arity), or (checked separately, in CheckExpr's
    // FunctionExpr case, since a closure never reaches this function at
    // all) a closure. `allowRestParam` is false for every caller of this
    // function except the one over program.functions - see each call
    // site's own comment.
    if (p.isRest && (!allowRestParam || !fn->typeParams.empty())) {
      Error(p.loc, "a rest parameter isn't supported here yet - only a plain, non-generic, top-level function "
                   "can have one");
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
  frameStack.push_back(decl);
  PushScope();
  for (auto &p : decl->params) Declare(p.loc, p.name, p.resolvedType, /*isConst=*/false, &p);
  // break/continue can never jump out of a nested function even if it's
  // lexically inside a loop textually (same as real JS) - reset both to
  // 0 for this body, restore afterward. Always 0 already here (a
  // top-level function/method is never itself inside a loop), but reset
  // explicitly anyway rather than relying on that, matching every other
  // body-checking site (see loopDepth's own doc comment for the other
  // three).
  int savedLoopDepth = loopDepth, savedSwitchDepth = switchDepth;
  loopDepth = 0;
  switchDepth = 0;
  CheckStmt(decl->body.get());
  loopDepth = savedLoopDepth;
  switchDepth = savedSwitchDepth;
  PopScope();
  frameStack.pop_back();
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

    inst->isExtern = tmpl->isExtern;

    if (tmpl->body) {
      inst->body = CloneStmt(*tmpl->body);
      FunctionDecl *savedCurrentFunction = currentFunction;
      currentFunction = inst;
      frameStack.push_back(inst);
      PushScope();
      for (auto &p : inst->params) Declare(p.loc, p.name, p.resolvedType, /*isConst=*/false, &p);
      int savedLoopDepth = loopDepth, savedSwitchDepth = switchDepth;
      loopDepth = 0;
      switchDepth = 0;
      CheckStmt(inst->body.get());
      loopDepth = savedLoopDepth;
      switchDepth = savedSwitchDepth;
      PopScope();
      frameStack.pop_back();
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
  // A throw never falls through past itself either - same "this code
  // path is accounted for" reasoning a real return already gets, and
  // matching real TS's own control-flow analysis (a function that
  // always throws satisfies "returns T" the same as one that always
  // returns a T).
  case StmtKind::Throw:
    return true;
  case StmtKind::Block:
    for (auto &s : stmt->statements)
      if (AlwaysReturns(s.get())) return true;
    return false;
  case StmtKind::If:
    return stmt->elseBranch && AlwaysReturns(stmt->body.get()) && AlwaysReturns(stmt->elseBranch.get());
  // Unlike While/For/ForOf (which might run their body zero times, so
  // the body always-returning wouldn't help - stays under the `default`
  // fallback below, same as always), a do-while's body ALWAYS runs at
  // least once - so if the body always returns, so does the whole loop,
  // regardless of the condition.
  case StmtKind::DoWhile:
    return AlwaysReturns(stmt->body.get());
  // A switch always-returns only if it has a `default:` arm (otherwise
  // an unmatched value falls through past it with no return) AND every
  // arm, checked independently (each arm's own statement list, the same
  // Block-style scan as above), always returns on its own. Deliberately
  // NOT fallthrough-aware - an arm that returns only because it falls
  // through into a later arm's own return is undercounted here (this
  // says "false" for it, not "true"), the same conservative-by-design
  // choice the rest of this function already makes elsewhere (see e.g.
  // If, which requires both an explicit `else` and both branches to
  // always return - no cleverness about detecting exhaustive conditions
  // any other way).
  case StmtKind::Switch: {
    bool hasDefault = false;
    for (auto &arm : stmt->statements) {
      if (!arm->expr) hasDefault = true;
    }
    if (!hasDefault) return false;
    for (auto &arm : stmt->statements) {
      bool armReturns = false;
      for (auto &s : arm->statements) {
        if (AlwaysReturns(s.get())) {
          armReturns = true;
          break;
        }
      }
      if (!armReturns) return false;
    }
    return true;
  }
  // A try always-returns only if BOTH the try body and the catch body
  // do - if either one can fall off the end, so can the whole
  // statement (either by completing the try normally with no
  // exception, or by an exception being caught and the catch body
  // itself not returning).
  case StmtKind::Try:
    return AlwaysReturns(stmt->body.get()) && AlwaysReturns(stmt->elseBranch.get());
  default:
    return false;
  }
}

// See this method's own doc comment in sema.h.
bool Sema::AlwaysExits(Stmt *stmt) {
  switch (stmt->kind) {
  case StmtKind::Return:
  case StmtKind::Throw:
  case StmtKind::Break:
  case StmtKind::Continue:
    return true;
  case StmtKind::Block:
    for (auto &s : stmt->statements)
      if (AlwaysExits(s.get())) return true;
    return false;
  case StmtKind::If:
    return stmt->elseBranch && AlwaysExits(stmt->body.get()) && AlwaysExits(stmt->elseBranch.get());
  default:
    return false;
  }
}

// See this method's own doc comment in sema.h.
bool Sema::TryGetNullCheckedVar(Expr *cond, std::string &outName, bool &outEqualsNull) {
  if (cond->kind != ExprKind::Binary) return false;
  if (cond->op != "==" && cond->op != "!=") return false;

  Expr *identSide = nullptr;
  if (cond->lhs->kind == ExprKind::Identifier && cond->rhs->kind == ExprKind::NullLiteral) {
    identSide = cond->lhs.get();
  } else if (cond->rhs->kind == ExprKind::Identifier && cond->lhs->kind == ExprKind::NullLiteral) {
    identSide = cond->rhs.get();
  } else {
    return false;
  }

  VarInfo *v = Lookup(identSide->name);
  if (!v || v->type.tag != TypeTag::Nullable) return false;

  outName = identSide->name;
  outEqualsNull = (cond->op == "==");
  return true;
}

// See this method's own doc comment in sema.h.
bool Sema::TryGetTypeofCheckedVar(Expr *cond, std::string &outName, AnyTag &outTag, bool &outEqualsTag) {
  if (cond->kind != ExprKind::Binary) return false;
  if (cond->op != "==" && cond->op != "!=") return false;

  auto isTypeofIdent = [](Expr *e) {
    return e->kind == ExprKind::Unary && e->op == "typeof" && e->operand->kind == ExprKind::Identifier;
  };

  Expr *typeofSide = nullptr;
  Expr *stringSide = nullptr;
  if (isTypeofIdent(cond->lhs.get()) && cond->rhs->kind == ExprKind::StringLiteral) {
    typeofSide = cond->lhs.get();
    stringSide = cond->rhs.get();
  } else if (isTypeofIdent(cond->rhs.get()) && cond->lhs->kind == ExprKind::StringLiteral) {
    typeofSide = cond->rhs.get();
    stringSide = cond->lhs.get();
  } else {
    return false;
  }

  Expr *ident = typeofSide->operand.get();
  VarInfo *v = Lookup(ident->name);
  if (!v || v->type.tag != TypeTag::Any) return false;

  const std::string &tagName = stringSide->name;
  // Only these three narrow anything - see TypeTag::Any's own doc
  // comment for exactly why "object"/"function" don't. The comparison
  // itself still type-checked fine either way (it's an ordinary string
  // equality - see CheckExpr's own Unary "typeof" case) - this just
  // declines to narrow, same as if the condition shape didn't match.
  if (tagName == "number") outTag = AnyTag::Number;
  else if (tagName == "boolean") outTag = AnyTag::Boolean;
  else if (tagName == "string") outTag = AnyTag::String;
  else return false;

  outName = ident->name;
  outEqualsTag = (cond->op == "==");
  return true;
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
    Declare(stmt->loc, stmt->varName, stmt->resolvedVarType, stmt->isConst, nullptr, stmt);
    break;
  }
  case StmtKind::If: {
    ResolvedType boolT = ResolvedType::Boolean();
    CheckExpr(stmt->cond.get(), &boolT);

    // Narrowing (see TryGetNullCheckedVar's/TryGetTypeofCheckedVar's own
    // doc comments for exactly which condition shapes these recognize at
    // all): `x != null`/`typeof x === "..."` narrows `x` for the `then`
    // body only; `x == null`/`typeof x !== "..."` with no `else`, where
    // the body always exits, narrows `x` for whatever code follows this
    // whole `if` in the SAME enclosing block instead (see
    // pendingNarrowAfterStmt/pendingAnyNarrowAfterStmt, read by
    // StmtKind::Block's own loop). A single `if` can only ever match ONE
    // of the two shapes - checked in order, and the second only tried
    // once the first has already ruled itself out.
    std::string narrowedName;
    bool checksEqualsNull = false;
    bool isNullCheck = TryGetNullCheckedVar(stmt->cond.get(), narrowedName, checksEqualsNull);

    std::string anyNarrowedName;
    AnyTag anyNarrowTag = AnyTag::Number;
    bool checksEqualsTag = false;
    bool isTypeofCheck =
        !isNullCheck && TryGetTypeofCheckedVar(stmt->cond.get(), anyNarrowedName, anyNarrowTag, checksEqualsTag);

    if (isNullCheck && !checksEqualsNull) {
      narrowedNonNull.insert(narrowedName);
      CheckStmt(stmt->body.get());
      narrowedNonNull.erase(narrowedName);
    } else if (isTypeofCheck && checksEqualsTag) {
      narrowedAny[anyNarrowedName] = anyNarrowTag;
      CheckStmt(stmt->body.get());
      narrowedAny.erase(anyNarrowedName);
    } else {
      CheckStmt(stmt->body.get());
    }

    if (stmt->elseBranch) {
      CheckStmt(stmt->elseBranch.get());
    } else if (isNullCheck && checksEqualsNull && AlwaysExits(stmt->body.get())) {
      pendingNarrowAfterStmt = narrowedName;
    } else if (isTypeofCheck && !checksEqualsTag && AlwaysExits(stmt->body.get())) {
      pendingAnyNarrowAfterStmt = anyNarrowedName;
      pendingAnyNarrowTag = anyNarrowTag;
    }
    break;
  }
  case StmtKind::While: {
    ResolvedType boolT = ResolvedType::Boolean();
    CheckExpr(stmt->cond.get(), &boolT);
    loopDepth++;
    CheckStmt(stmt->body.get());
    loopDepth--;
    break;
  }
  case StmtKind::DoWhile: {
    // Same checks as While, just body-then-condition - see ast.h's own
    // doc comment on StmtKind::DoWhile for why this reuses While's exact
    // fields instead of adding its own.
    loopDepth++;
    CheckStmt(stmt->body.get());
    loopDepth--;
    ResolvedType boolT = ResolvedType::Boolean();
    CheckExpr(stmt->cond.get(), &boolT);
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
    loopDepth++;
    CheckStmt(stmt->body.get());
    loopDepth--;
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
    Declare(stmt->loc, stmt->varName, elemT, stmt->isConst, nullptr, stmt);
    loopDepth++;
    CheckStmt(stmt->body.get());
    loopDepth--;
    PopScope();
    break;
  }
  case StmtKind::Break: {
    if (loopDepth == 0 && switchDepth == 0) {
      Error(stmt->loc, "'break' outside a loop or switch");
    }
    break;
  }
  case StmtKind::Continue: {
    if (loopDepth == 0) {
      Error(stmt->loc, "'continue' outside a loop");
    }
    break;
  }
  case StmtKind::Switch: {
    ResolvedType discriminantT = CheckExpr(stmt->expr.get(), nullptr);
    switchDepth++;
    // No PushScope/PopScope per-arm - a switch's arms deliberately share
    // one scope (see StmtKind::Case's own doc comment: a `let` in one
    // arm stays visible to a later one it falls through into, matching
    // real JS). One shared scope for the whole switch body lets every
    // arm's own statements see earlier arms' locals without them leaking
    // past the switch entirely.
    PushScope();
    for (auto &arm : stmt->statements) {
      // A case value must match the switch's own discriminant type -
      // checked directly here (against discriminantT), not by
      // generically dispatching to StmtKind::Case below, since CheckStmt
      // has no "expected type" parameter to thread that through.
      // `arm->expr` is null for `default:` (see ParseSwitch).
      if (arm->expr) {
        CheckExpr(arm->expr.get(), discriminantT.tag != TypeTag::Unknown ? &discriminantT : nullptr);
      }
      for (auto &s : arm->statements) CheckStmt(s.get());
    }
    PopScope();
    switchDepth--;
    break;
  }
  case StmtKind::Case: {
    // Only ever reached through StmtKind::Switch's own loop above, which
    // handles a Case arm's expr/statements directly so it can check the
    // arm's value against the switch's own discriminant type. A Case
    // node is never on any other AST path, so this is unreachable in
    // practice; still handled (rather than omitted) so this switch stays
    // exhaustive, and so it degrades safely instead of silently
    // no-op'ing if that ever stops being true.
    if (stmt->expr) CheckExpr(stmt->expr.get(), nullptr);
    for (auto &s : stmt->statements) CheckStmt(s.get());
    break;
  }
  case StmtKind::Destructure: {
    ResolvedType srcT = CheckExpr(stmt->expr.get(), nullptr);
    if (srcT.tag != TypeTag::Struct) {
      if (srcT.tag != TypeTag::Unknown) {
        Error(stmt->loc, "cannot destructure a value of type '" + srcT.ToString() +
                              "' - only an interface/class instance can be destructured");
      }
      break;
    }
    InterfaceDecl *iface = interfaces.at(srcT.structName);
    std::unordered_set<std::string> seenLocals;
    for (auto &b : stmt->destructureBindings) {
      const InterfaceField *field = FindField(iface, b.fieldName);
      if (!field) {
        std::string hint;
        if (FindGetter(iface, b.fieldName) || FindSetter(iface, b.fieldName)) {
          hint = " (it's a get/set property, not a plain field - destructuring only reads plain fields)";
        } else if (FindPlainMethod(iface, b.fieldName)) {
          hint = " (it's a method, not a field)";
        }
        Error(b.loc, "interface '" + iface->name + "' has no field '" + b.fieldName + "'" + hint);
        continue;
      }
      if (!seenLocals.insert(b.localName).second) {
        Error(b.loc, "'" + b.localName + "' is already bound by this destructuring pattern");
        continue;
      }
      b.resolvedType = field->resolvedType;
      Declare(b.loc, b.localName, b.resolvedType, stmt->isConst, nullptr, stmt);
    }
    break;
  }
  case StmtKind::Try: {
    // Body is always a Block (see Parser::ParseTry) - CheckStmt's own
    // Block case already pushes/pops its own scope, so nothing extra
    // needed here for it.
    CheckStmt(stmt->body.get());

    ResolvedType catchType = ResolveType(stmt->declaredType.get());
    if (catchType.tag != TypeTag::Unknown &&
        !(catchType.tag == TypeTag::Struct && catchType.structName == "Error")) {
      Error(stmt->loc, "a catch clause can only catch 'Error' right now - '" + catchType.ToString() +
                            "' isn't a throwable/catchable type yet (only one exception type exists so "
                            "far - see README.md's own note on why)");
    }
    stmt->resolvedVarType = catchType;
    // One scope for the catch variable itself, wrapping the catch
    // body's own Block (which pushes its own separate inner scope for
    // its own statements) - same shape a function body's own params +
    // Block already has (see CheckFunctionBody).
    PushScope();
    Declare(stmt->loc, stmt->varName, catchType, /*isConst=*/false, nullptr, stmt);
    CheckStmt(stmt->elseBranch.get());
    PopScope();
    break;
  }
  case StmtKind::Throw: {
    ResolvedType errorType = ResolvedType::Struct("Error");
    CheckExpr(stmt->expr.get(), &errorType);
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
  case StmtKind::Block: {
    PushScope();
    // Names THIS block's own loop narrowed (via an `if (x == null) {
    // <exits> }` with no `else` - see pendingNarrowAfterStmt) - undone
    // when the block itself ends, so a narrow never leaks past the
    // block it was established in. `.insert(...).second` guards against
    // un-narrowing a name an OUTER block already had narrowed before
    // this one even started (rare, but real: a block nested inside an
    // already-narrowed `if (x != null) { ... }`, say) - only a name
    // THIS loop actually added gets removed again here.
    std::vector<std::string> narrowedHere;
    std::vector<std::string> anyNarrowedHere;
    for (auto &s : stmt->statements) {
      CheckStmt(s.get());
      if (!pendingNarrowAfterStmt.empty()) {
        if (narrowedNonNull.insert(pendingNarrowAfterStmt).second) {
          narrowedHere.push_back(pendingNarrowAfterStmt);
        }
        pendingNarrowAfterStmt.clear();
      }
      // Same deal, for the `any`-flavored `typeof x !== "..."` early-exit
      // shape (see pendingAnyNarrowAfterStmt's own doc comment) - a
      // parallel, independent check since a single statement only ever
      // sets one of the two pending fields, never both.
      if (!pendingAnyNarrowAfterStmt.empty()) {
        if (narrowedAny.find(pendingAnyNarrowAfterStmt) == narrowedAny.end()) {
          narrowedAny[pendingAnyNarrowAfterStmt] = pendingAnyNarrowTag;
          anyNarrowedHere.push_back(pendingAnyNarrowAfterStmt);
        }
        pendingAnyNarrowAfterStmt.clear();
      }
    }
    for (auto &name : narrowedHere) narrowedNonNull.erase(name);
    for (auto &name : anyNarrowedHere) narrowedAny.erase(name);
    PopScope();
    break;
  }
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
    if (target->lhs->kind == ExprKind::Identifier && IsStaticAccessTarget(target->lhs->name)) {
      // `ClassName.staticField = value`/`EnumName.member = value` - same
      // rewrite-to-a-plain-Identifier trick CheckExpr's own Member case
      // uses for a read (see its own comment). A static field is always
      // a plain assignable global, never setter-backed (there's no
      // "static get"/"static set" - see ParseClassBody), so there's no
      // setter-vs-field split to make here the way there is below - and
      // an enum member is always a `const` one (see the enum
      // registration pass in Check()), so this always rejects it, the
      // same path an ordinary `const` global's own reassignment already
      // takes.
      const std::string &nsName = target->lhs->name;
      auto ifaceIt = interfaces.find(nsName);
      std::string mangled = ifaceIt != interfaces.end() ? MangleStatic(ifaceIt->second->name, target->name)
                                                         : MangleEnumMember(enums.at(nsName)->name, target->name);
      if (globals.count(mangled)) {
        target->kind = ExprKind::Identifier;
        target->name = mangled;
        VarInfo &v = globals.at(mangled);
        if (v.isConst) Error(opLoc, "cannot assign to '" + mangled.substr(mangled.rfind('$') + 1) +
                                         "' - it is declared 'const'");
        target->resolvedType = v.type;
        return target->resolvedType;
      }
      if (ifaceIt != interfaces.end() && functions.count(mangled)) {
        Error(opLoc, "'" + target->name + "' is a static method of '" + ifaceIt->second->name + "', not assignable");
      } else {
        std::string kind = ifaceIt != interfaces.end() ? "class" : "enum";
        Error(opLoc, kind + " '" + nsName + "' has no member '" + target->name + "'");
      }
      target->resolvedType = ResolvedType{};
      return target->resolvedType;
    }
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
        if (field->isReadonly) {
          Error(opLoc, "cannot assign to '" + target->name + "' - it is declared 'readonly'");
        }
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

bool Sema::IsAssignable(const ResolvedType &from, const ResolvedType &to) {
  if (from == to) return true;
  if (from.tag == TypeTag::Struct && to.tag == TypeTag::Struct) {
    for (InterfaceDecl *cur = interfaces.at(from.structName); cur; cur = cur->baseClass) {
      if (cur->name == to.structName) return true;
    }
  }
  return false;
}

// See this method's own doc comment in sema.h.
bool Sema::IsAnyBoxable(TypeTag t) {
  switch (t) {
  case TypeTag::Number:
  case TypeTag::Boolean:
  case TypeTag::String:
  case TypeTag::Struct:
  case TypeTag::Array:
  case TypeTag::Handler:
  case TypeTag::Enum:
    return true;
  default:
    return false;
  }
}

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

  case ExprKind::NullLiteral:
    // Same "can't infer from nothing" deal an empty array literal has -
    // `null` alone carries no T of its own, so it always needs an
    // expected Nullable(T) to resolve against (a `let`'s own declared
    // type, a parameter/return type, ...). The generic actual-vs-
    // expected check at the tail of this function already accepts this
    // once `actual` here matches `expected` exactly (see IsAssignable -
    // `null` widens to nothing else, it only ever fits the EXACT
    // Nullable(T) it's being checked against).
    if (expected && expected->tag == TypeTag::Nullable) {
      actual = *expected;
    } else if (expected && expected->tag == TypeTag::Any) {
      // `any` absorbs `null` directly (see TypeTag::Any's own doc
      // comment) - a real, distinct AnyTag::Null box, not the bare null
      // pointer a Nullable(T)'s own "absent" value is (see Codegen's own
      // NullLiteral case), so `typeof` still has a real box to read a tag
      // out of. No needsAnyBox here: unlike every other boxable value,
      // there's no "concrete pre-box value" to box in the first place.
      actual = ResolvedType::Any();
    } else {
      Error(expr->loc, "cannot infer a type for 'null' - add an explicit 'T | null' type annotation");
      actual = ResolvedType{};
    }
    break;

  case ExprKind::Identifier: {
    VarInfo *v = Lookup(expr->name);
    auto funcIt = functions.find(expr->name);
    if (v) {
      // Narrowing (see narrowedNonNull's own doc comment): proven
      // non-null AT THIS SPECIFIC REFERENCE, not a blanket fact about
      // the variable - a later reference to the same name, once out of
      // the narrowed region, resolves as plain Nullable(T) again, same
      // as if this check had never happened.
      auto anyNarrow = narrowedAny.find(expr->name);
      if (v->type.tag == TypeTag::Nullable && narrowedNonNull.count(expr->name)) {
        actual = *v->type.elementType;
        expr->isNarrowedNonNull = true;
      } else if (v->type.tag == TypeTag::Any && anyNarrow != narrowedAny.end()) {
        // Narrowing (see narrowedAny's own doc comment): proven to hold
        // one of the three narrowable concrete shapes AT THIS SPECIFIC
        // REFERENCE, same per-reference-not-per-declaration discipline
        // narrowedNonNull already has.
        switch (anyNarrow->second) {
        case AnyTag::Number: actual = ResolvedType::Number(); break;
        case AnyTag::Boolean: actual = ResolvedType::Boolean(); break;
        case AnyTag::String: actual = ResolvedType::String(); break;
        default: actual = v->type; break; // unreachable - narrowedAny never holds Object/Function/Null (see TryGetTypeofCheckedVar)
        }
        expr->isNarrowedAny = true;
      } else {
        actual = v->type;
      }
    } else if (funcIt != functions.end() && IsVisible(expr->name)) {
      // A bare function name (not a call) - a plain code address with no
      // captured environment of its own (contrast a closure literal,
      // ExprKind::FunctionExpr, which can capture - see its own case
      // below). Only a void-returning function can be used this way -
      // its own parameter types become the Handler's, checked
      // structurally like any other type against whatever
      // "(params...) => void" the use site actually expects.
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
               ambient != kAmbientGlobals.end() && functions.count(ambient->second)) {
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
      //
      // Deliberately no IsVisible(ambient->second) check here, unlike
      // every other name lookup in this function: `document` is ambient
      // precisely because it needs no import to use, so its backing
      // function can't be gated behind one either, or a multi-file
      // project where the DOM bridge lives in its own file (see
      // README.md's "Using ART" section) would need to import a function
      // (`ArtDocument`) it never actually calls by name, just to make
      // this sugar keep working. Existing anywhere in the merged program
      // is enough - the same "ambient, not import-gated" treatment
      // `IsVisible` already gives every real builtin via `builtinNames`.
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
    const std::string &op = expr->op;
    // `x != null`/`x == null` - the ONLY legal use of a bare `null`
    // outside of somewhere with a real declared Nullable(T) type to
    // check it against (a `let`'s own annotation, notNull<T>'s return
    // type, ...). Checked as a special case, before the generic
    // "check both sides with no expected type" path below: a bare
    // NullLiteral has NOTHING to infer a type from on its own (see its
    // own CheckExpr case) - it needs the OTHER side's already-resolved
    // type handed to it as `expected`, which only this operator
    // (uniquely, among every Binary one) can actually provide.
    bool lhsIsNull = expr->lhs->kind == ExprKind::NullLiteral;
    bool rhsIsNull = expr->rhs->kind == ExprKind::NullLiteral;
    if ((op == "==" || op == "!=") && (lhsIsNull || rhsIsNull) && !(lhsIsNull && rhsIsNull)) {
      Expr *nullSide = lhsIsNull ? expr->lhs.get() : expr->rhs.get();
      Expr *otherSide = lhsIsNull ? expr->rhs.get() : expr->lhs.get();
      ResolvedType otherT = CheckExpr(otherSide, nullptr);
      if (otherT.tag != TypeTag::Nullable && otherT.tag != TypeTag::Unknown) {
        Error(expr->loc, "operator '" + op + "' against 'null' only makes sense for a 'T | null' value, found '" +
                              otherT.ToString() + "'");
      }
      CheckExpr(nullSide, otherT.tag == TypeTag::Nullable ? &otherT : nullptr);
      actual = ResolvedType::Boolean();
      break;
    }
    ResolvedType lhsT = CheckExpr(expr->lhs.get(), nullptr);
    ResolvedType rhsT = CheckExpr(expr->rhs.get(), nullptr);
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
    } else if (op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>" || op == ">>>") {
      // Same "real double, ToInt32-truncated at the operator itself"
      // deal real JS bitwise/shift operators have - see Codegen's own
      // Binary case for the actual truncate/operate/widen-back sequence.
      // Always yields a number either way, same shape every other
      // numeric operator here has.
      if (!lhsUnknown && !rhsUnknown && (lhsT.tag != TypeTag::Number || rhsT.tag != TypeTag::Number)) {
        Error(expr->loc, "operator '" + op + "' requires two numbers, found '" + lhsT.ToString() + "' and '" +
                              rhsT.ToString() + "'");
      }
      actual = ResolvedType::Number();
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
    } else if (expr->op == "~") {
      if (!unknown && operandT.tag != TypeTag::Number) {
        Error(expr->loc, "unary '~' requires a number, found '" + operandT.ToString() + "'");
      }
      actual = ResolvedType::Number();
    } else if (expr->op == "typeof") {
      // Valid only on an `any`-typed operand - see TypeTag::Any's own doc
      // comment. Always yields `string`, one of "number"/"boolean"/
      // "string"/"object"/"function" (see AnyTag) - the actual runtime
      // dispatch lives in Codegen's own Unary case, not here; Sema only
      // needs to know this always produces a string, so `typeof x ===
      // "..."` type-checks as an ordinary string equality with no special
      // casing of its own (see Binary's case above).
      if (!unknown && operandT.tag != TypeTag::Any) {
        Error(expr->loc, "'typeof' requires an 'any' value, found '" + operandT.ToString() + "'");
      }
      actual = ResolvedType::String();
    } else {
      Error(expr->loc, "unknown unary operator '" + expr->op + "'");
      actual = ResolvedType{};
    }
    break;
  }

  case ExprKind::Call: {
    if (expr->lhs->kind == ExprKind::Member) {
      Expr *memberExpr = expr->lhs.get();
      if (memberExpr->lhs->kind == ExprKind::Identifier && IsStaticAccessTarget(memberExpr->lhs->name)) {
        // Captured into locals before `expr->lhs` is ever reassigned
        // below (which frees the Member node `memberExpr` points into -
        // reading memberExpr->name/loc afterward would be a
        // use-after-free).
        std::string nsName = memberExpr->lhs->name;
        std::string propName = memberExpr->name;
        SourceLoc propLoc = memberExpr->loc;

        auto ifaceIt = interfaces.find(nsName);
        if (ifaceIt == interfaces.end()) {
          // `EnumName.member(...)` - an enum has no callable members at
          // all (see EnumDecl's own doc comment: arithmetic and every
          // other operation beyond '=='/'!=' is deliberately unsupported
          // on an enum-typed value, and there's certainly no such thing
          // as an enum *method*).
          EnumDecl *enumDecl = enums.at(nsName);
          bool exists = globals.count(MangleEnumMember(enumDecl->name, propName)) != 0;
          if (exists) {
            Error(expr->loc, "'" + propName + "' is a member of enum '" + enumDecl->name + "', not callable");
          } else {
            Error(expr->loc, "enum '" + enumDecl->name + "' has no member '" + propName + "'");
          }
          for (auto &arg : expr->elements) CheckExpr(arg.get(), nullptr);
          actual = ResolvedType{};
          break;
        }
        // `ClassName.staticMethod(args)` - unlike an instance method call
        // below, there's no receiver to splice in at all: this is really
        // just an ordinary call to the mangled top-level function (see
        // Sema::MangleStatic), so `expr->lhs` is rewritten to a plain
        // Identifier naming it - the exact shape an ordinary named-
        // function call already has by the time Codegen sees it, so
        // Codegen's own Call case needs no new branch for this at all.
        InterfaceDecl *iface = ifaceIt->second;
        std::string mangled = MangleStatic(iface->name, propName);
        if (!expr->typeArgs.empty()) {
          Error(expr->loc, "a method call can't take explicit type arguments yet");
        }
        auto it = functions.find(mangled);
        if (it == functions.end()) {
          if (globals.count(mangled)) {
            Error(expr->loc, "'" + propName + "' is a static field of '" + iface->name + "', not callable");
          } else {
            Error(expr->loc, "class '" + iface->name + "' has no static member '" + propName + "'");
          }
          for (auto &arg : expr->elements) CheckExpr(arg.get(), nullptr);
          actual = ResolvedType{};
          break;
        }
        FunctionDecl *fn = it->second;
        expr->resolvedCalleeName = mangled;
        auto newLhs = MakeExpr(ExprKind::Identifier, propLoc);
        newLhs->name = mangled;
        expr->lhs = std::move(newLhs); // discards the old Member(ClassName, method) subtree
        if (fn->params.size() != expr->elements.size()) {
          Error(expr->loc, "static method '" + propName + "' expects " + std::to_string(fn->params.size()) +
                                " argument(s), got " + std::to_string(expr->elements.size()));
        }
        size_t n = std::min(fn->params.size(), expr->elements.size());
        for (size_t i = 0; i < n; i++) CheckExpr(expr->elements[i].get(), &fn->params[i].resolvedType);
        for (size_t i = n; i < expr->elements.size(); i++) CheckExpr(expr->elements[i].get(), nullptr);
        actual = fn->resolvedReturnType;
        break;
      }
      // `obj.method(args)` - always resolved statically against obj's
      // declared type (no vtable/dynamic dispatch - see
      // InterfaceDecl::methods' own doc comment), so this is pure
      // call-site sugar for a plain call to the class's already-
      // qualified method function, with obj spliced in as the actual
      // first argument once every other argument has been checked.
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
      // Real dynamic dispatch for a method whose class is actually part
      // of an inheritance family (see FunctionDecl::isVirtual's own doc
      // comment) - `method->vtableSlot` is the same slot number no
      // matter which class's own override FindPlainMethod happened to
      // find statically here, so Codegen dispatching through it (via
      // the receiver's own RUNTIME vtable pointer, not this specific
      // resolvedCalleeName) reaches whichever override actually applies
      // to the real, dynamic instance - see Expr::virtualSlot's own doc
      // comment.
      expr->virtualSlot = method->isVirtual ? method->vtableSlot : -1;
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
        bool hasRestParam = !fn->params.empty() && fn->params.back().isRest;
        if (hasRestParam) {
          // Every argument from the rest parameter's own position
          // onward is optional (zero or more) - only the LEADING, fixed
          // parameters are actually required. Each trailing argument is
          // checked against the rest parameter's own ELEMENT type (not
          // the array type itself - a call site supplies individual
          // values, never a literal array, in this first pass; see
          // Param::isRest's own doc comment on the spread-in-calls gap
          // this leaves), matching how Codegen collects them into a
          // real array one at a time (see its own Call case).
          size_t fixedCount = fn->params.size() - 1;
          if (expr->elements.size() < fixedCount) {
            Error(expr->loc, "function '" + callee + "' expects at least " + std::to_string(fixedCount) +
                                  " argument(s), got " + std::to_string(expr->elements.size()));
          }
          size_t n = std::min(fixedCount, expr->elements.size());
          for (size_t i = 0; i < n; i++) CheckExpr(expr->elements[i].get(), &fn->params[i].resolvedType);
          const ResolvedType &elemType = *fn->params.back().resolvedType.elementType;
          for (size_t i = fixedCount; i < expr->elements.size(); i++) {
            CheckExpr(expr->elements[i].get(), &elemType);
          }
        } else {
          if (fn->params.size() != expr->elements.size()) {
            Error(expr->loc, "function '" + callee + "' expects " + std::to_string(fn->params.size()) +
                                  " argument(s), got " + std::to_string(expr->elements.size()));
          }
          size_t n = std::min(fn->params.size(), expr->elements.size());
          for (size_t i = 0; i < n; i++) CheckExpr(expr->elements[i].get(), &fn->params[i].resolvedType);
          for (size_t i = n; i < expr->elements.size(); i++) CheckExpr(expr->elements[i].get(), nullptr);
        }
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

  case ExprKind::JsxElement: {
    // Desugars to ART's standard library's own raw DOM bridge functions
    // (ArtCreateElement/ArtSetAttribute/ArtAppendChild/ArtAddEventListener/
    // ArtCreateTextNode - see art/stdlib/art.ts and Codegen's own
    // JsxElement case) - looked up directly rather than through the
    // normal functions/IsVisible path, the same "ambient, not import-
    // gated" treatment the `document` sugar above already gets, since
    // these are never written by name in the source that triggers them.
    // They exist in the merged program as soon as anything in the
    // project imports Node/Event from "art" - which JSX already implies,
    // since its own result type (below) needs Node to be a real,
    // resolvable type for the surrounding code (a `let`'s declared type,
    // a function's return type, ...) to type-check at all.
    static const char *kRequiredJsxFunctions[] = {"ArtCreateElement",   "ArtSetAttribute",  "ArtAppendChild",
                                                   "ArtAddEventListener", "ArtCreateTextNode", "numberToString"};
    bool stdlibAvailable = true;
    for (const char *name : kRequiredJsxFunctions) {
      if (!functions.count(name)) {
        stdlibAvailable = false;
        break;
      }
    }
    if (!stdlibAvailable) {
      Error(expr->loc, "JSX needs ART's standard library - add 'import { Node, Event } from \"art\";' to "
                        "this file (or a file it transitively imports)");
    } else {
      // Every attribute value must resolve to `string`, matching real
      // HTML attributes - no implicit stringification of e.g. a number,
      // same "explicit, not automatic" conversion rule everywhere else in
      // ART already has (write numberToString(x) yourself) - except an
      // "on<type>" attribute (onclick, onkeydown, ...), which must be a
      // `(event: Event) => void` handler instead, desugaring to
      // .addEventListener(type, handler, false).
      ResolvedType stringType = ResolvedType::String();
      ResolvedType handlerType = ResolvedType::Handler({ResolvedType::Struct("Event")});
      for (auto &attr : expr->fields) {
        bool isEventAttr = attr.first.size() > 2 && attr.first.rfind("on", 0) == 0;
        CheckExpr(attr.second.get(), isEventAttr ? &handlerType : &stringType);
      }
      // A child is a nested JsxElement/other Node-typed expression
      // (appended as-is), a string/number `{ expr }` (auto-converted to
      // a text node, via numberToString first for a number), or a
      // `Node[]` (spread - each element appended in its own right, for a
      // dynamically-sized list of children a fixed `{a}{b}{c}` list can't
      // express - see Codegen's own JsxElement case for the runtime loop
      // this compiles to). Anything else (including boolean, or a `T[]`
      // of some other T) is a compile error, not a silent stringify.
      for (auto &child : expr->elements) {
        ResolvedType childT = CheckExpr(child.get(), nullptr);
        bool isNode = childT.tag == TypeTag::Struct && childT.structName == "Node";
        bool isNodeArray = childT.tag == TypeTag::Array && childT.elementType->tag == TypeTag::Struct &&
                            childT.elementType->structName == "Node";
        if (childT.tag != TypeTag::String && childT.tag != TypeTag::Number && !isNode && !isNodeArray) {
          Error(child->loc,
                "a JSX child must be a string, number, Node, or Node[], found '" + childT.ToString() + "'");
        }
      }
    }
    // A fragment (`<>...</>` - see ExprKind::JsxElement's own doc
    // comment) has no wrapping element, so it's `Node[]` - an ordered
    // group of its children - rather than `Node`. `expr->fields` is
    // always empty for one (the parser never lets a fragment have
    // attributes), so the attribute loop above is already a no-op there.
    bool isFragment = expr->name.empty();
    actual = isFragment ? ResolvedType::ArrayOf(ResolvedType::Struct("Node")) : ResolvedType::Struct("Node");
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
    if (expr->lhs->kind == ExprKind::Identifier && IsStaticAccessTarget(expr->lhs->name)) {
      // `ClassName.member`/`EnumName.member` - a static field/method or
      // enum member access, not an instance one on some variable (there
      // is none - see IsStaticAccessTarget). Rewritten in place into a
      // plain Identifier naming the mangled global (see
      // Sema::MangleStatic/MangleEnumMember) - reuses every existing
      // Identifier lookup/lvalue/codegen path for a global with no new
      // Codegen case at all, the same trick a getter's own Member->Call
      // rewrite below uses for methods.
      const std::string &nsName = expr->lhs->name;
      auto ifaceIt = interfaces.find(nsName);
      if (ifaceIt != interfaces.end()) {
        InterfaceDecl *iface = ifaceIt->second;
        std::string mangled = MangleStatic(iface->name, expr->name);
        if (globals.count(mangled)) {
          expr->kind = ExprKind::Identifier;
          expr->name = mangled;
          actual = globals.at(mangled).type;
        } else if (functions.count(mangled)) {
          Error(expr->loc, "'" + expr->name + "' is a static method of '" + iface->name +
                                "' - it can only be used with call syntax ('(...)')");
          actual = ResolvedType{};
        } else {
          Error(expr->loc, "class '" + iface->name + "' has no static member '" + expr->name + "'");
          actual = ResolvedType{};
        }
      } else {
        EnumDecl *enumDecl = enums.at(nsName);
        std::string mangled = MangleEnumMember(enumDecl->name, expr->name);
        if (globals.count(mangled)) {
          expr->kind = ExprKind::Identifier;
          expr->name = mangled;
          actual = globals.at(mangled).type;
        } else {
          Error(expr->loc, "enum '" + enumDecl->name + "' has no member '" + expr->name + "'");
          actual = ResolvedType{};
        }
      }
      break;
    }
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
    // A narrowed `x != null` guard only proves `x` was non-null at the
    // check - reassigning `x` inside that narrowed region (to `null` or
    // even back to a non-null value) makes the guard stale, since
    // Codegen's unboxing decision is baked into each Identifier read at
    // Sema time, not re-verified at runtime. Un-narrow on any write to
    // avoid unboxing a pointer that's no longer known non-null.
    if (expr->lhs->kind == ExprKind::Identifier) {
      narrowedNonNull.erase(expr->lhs->name);
      narrowedAny.erase(expr->lhs->name);
    }
    actual = targetT;
    break;
  }

  case ExprKind::Conditional: {
    ResolvedType boolType = ResolvedType::Boolean();
    CheckExpr(expr->operand.get(), &boolType);
    // No union types (see README.md's "What's not in ART") - both
    // branches must resolve to the exact same type, not just something
    // wide enough to cover either. With an expected type (e.g. a `let`'s
    // own declared type), check both branches against it directly, the
    // same way ArrayLiteral checks every element against an expected
    // array's element type; with none, infer from the then-branch and
    // check the else-branch against that, the same way ArrayLiteral
    // infers its element type from the first element when there's
    // nothing else to go on.
    if (expected) {
      CheckExpr(expr->lhs.get(), expected);
      CheckExpr(expr->rhs.get(), expected);
      actual = *expected;
    } else {
      ResolvedType thenT = CheckExpr(expr->lhs.get(), nullptr);
      CheckExpr(expr->rhs.get(), &thenT);
      actual = thenT;
    }
    break;
  }

  case ExprKind::TemplateLiteral: {
    // Literal-text parts (even indices) are always real StringLiteral
    // nodes already - checking them is trivial, but done uniformly with
    // the interpolated ones (odd indices) for consistency. Each
    // interpolated expression must resolve to `string` or `number` - a
    // `number` gets stringified via `numberToString` in Codegen, same
    // rule (and same reason: no universal to-string, no implicit
    // conversion beyond that one documented case) JSX's own children
    // already have.
    for (size_t i = 0; i < expr->elements.size(); i++) {
      ResolvedType partT = CheckExpr(expr->elements[i].get(), nullptr);
      if (i % 2 == 1 && partT.tag != TypeTag::String && partT.tag != TypeTag::Number) {
        Error(expr->elements[i]->loc, "a template literal's '${...}' must be a string or number, found '" +
                                           partT.ToString() + "'");
      }
    }
    actual = ResolvedType::String();
    break;
  }

  case ExprKind::FunctionExpr: {
    // An anonymous closure literal - see ast.h's own doc comment on
    // Expr::fn. Named/typechecked/body-checked fresh right here, exactly
    // once per AST node actually reached (so a generic template's own,
    // never-checked-in-template-form closure never gets a name, and each
    // concrete instantiation's own clone gets its own separate one when
    // Sema checks *it* - see Sema::Closures()). Captures - which locals
    // from an enclosing frame this closure references - are discovered
    // as a side effect of Lookup while checking the body below, not
    // computed up front; see Lookup's own doc comment.
    FunctionDecl *fn = expr->fn.get();
    fn->name = "$closure" + std::to_string(nextClosureId++);
    fn->sourceFile = currentFile;

    for (auto &p : fn->params) {
      p.resolvedType = ResolveType(p.type.get());
      // Not supported on a closure in this first pass either - see
      // RegisterFunctionSignature's own comment on allowRestParam.
      if (p.isRest) {
        Error(p.loc, "a rest parameter isn't supported here yet - only a plain, non-generic, top-level function "
                     "can have one");
      }
    }
    fn->resolvedReturnType = ResolveType(fn->returnType.get());
    if (fn->resolvedReturnType.tag != TypeTag::Void) {
      Error(expr->loc, "a function expression must return void - ART's Handler type ('(...) => void'-shaped "
                        "values) only supports void-returning functions, matching every actual use (event "
                        "handlers, timers, animation frames)");
    }

    FunctionDecl *savedCurrentFunction = currentFunction;
    currentFunction = fn;
    frameStack.push_back(fn);
    PushScope();
    for (auto &p : fn->params) Declare(p.loc, p.name, p.resolvedType, /*isConst=*/false, &p);
    int savedLoopDepth = loopDepth, savedSwitchDepth = switchDepth;
    loopDepth = 0;
    switchDepth = 0;
    CheckStmt(fn->body.get());
    loopDepth = savedLoopDepth;
    switchDepth = savedSwitchDepth;
    PopScope();
    frameStack.pop_back();
    currentFunction = savedCurrentFunction;
    // No AlwaysReturns check - a closure is always void (enforced just
    // above), the same "nothing to check" deal every other void
    // function's own body already has (see CheckFunctionBody's own
    // check, gated on resolvedReturnType.tag != TypeTag::Void).

    closures.push_back(fn);
    std::vector<ResolvedType> paramTypes;
    paramTypes.reserve(fn->params.size());
    for (auto &p : fn->params) paramTypes.push_back(p.resolvedType);
    actual = ResolvedType::Handler(std::move(paramTypes));
    break;
  }
  }

  // Implicit widening into `any` (see TypeTag::Any's own doc comment for
  // why this - unlike Nullable(T)'s own no-implicit-widening rule - is
  // deliberately unconditional for every boxable tag): once `actual` is
  // rewritten to Any here, the ordinary actual-vs-expected check right
  // below sees a plain Any-vs-Any match and passes with no error, the
  // same way an already-Any `actual` (e.g. reading an `any` variable)
  // already would. `preBoxType` remembers what `actual` was a moment ago
  // so Codegen's GenBoxAny knows what it's actually boxing.
  if (expected && expected->tag == TypeTag::Any && IsAnyBoxable(actual.tag)) {
    expr->needsAnyBox = true;
    expr->preBoxType = actual;
    actual = ResolvedType::Any();
  }

  expr->resolvedType = actual;
  if (expected && actual.tag != TypeTag::Unknown && expected->tag != TypeTag::Unknown &&
      !IsAssignable(actual, *expected)) {
    Error(expr->loc, "type mismatch: expected '" + expected->ToString() + "', found '" + actual.ToString() + "'");
  }
  return actual;
}

} // namespace ART
