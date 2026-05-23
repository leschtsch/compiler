#pragma once

#include <cstdint>

namespace semantics {

enum class BasicTypes : std::uint8_t {
  kChar,
  kInt,
  kUint,
  kFloat,
  kNBasicTypes
};

using TypeInfo = BasicTypes;

inline BasicTypes CommonType(BasicTypes lhs, BasicTypes rhs) {
  if (lhs == BasicTypes::kFloat || rhs == BasicTypes::kFloat) {
    return BasicTypes::kFloat;
  }
  if (lhs == BasicTypes::kUint || rhs == BasicTypes::kUint) {
    return BasicTypes::kUint;
  }
  if (lhs == BasicTypes::kInt || rhs == BasicTypes::kInt) {
    return BasicTypes::kInt;
  }

  return BasicTypes::kChar;
}

inline bool IsFloatingPoint(BasicTypes type) {
  return type == BasicTypes::kFloat;
}
inline bool IsInteger(BasicTypes type) { return !IsFloatingPoint(type); }

inline bool IsUnsigned(BasicTypes type) {
  return IsInteger(type) && type == BasicTypes::kUint;
}
inline bool IsSigned(BasicTypes type) {
  return IsInteger(type) && !IsUnsigned(type);
}

}  // namespace semantics
