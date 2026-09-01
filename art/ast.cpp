#include "ast.h"

namespace ART {

bool ResolvedType::operator==(const ResolvedType &other) const {
  if (tag != other.tag) return false;
  switch (tag) {
  case TypeTag::Array:
    return *elementType == *other.elementType;
  case TypeTag::Struct:
    return structName == other.structName;
  case TypeTag::Handler:
    return *handlerParamTypes == *other.handlerParamTypes;
  default:
    return true;
  }
}

std::string ResolvedType::ToString() const {
  switch (tag) {
  case TypeTag::Unknown: return "<unknown>";
  case TypeTag::Number: return "number";
  case TypeTag::Boolean: return "boolean";
  case TypeTag::String: return "string";
  case TypeTag::Void: return "void";
  case TypeTag::Array: return elementType->ToString() + "[]";
  case TypeTag::Struct: return structName;
  case TypeTag::Handler: {
    std::string out = "(";
    for (size_t i = 0; i < handlerParamTypes->size(); i++) {
      if (i > 0) out += ", ";
      out += (*handlerParamTypes)[i].ToString();
    }
    return out + ") => void";
  }
  }
  return "?";
}

} // namespace ART
