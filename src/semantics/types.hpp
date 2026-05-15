#pragma once

#include <cstdint>

namespace semantics {

using Char = char;
using Int = std::int64_t;
using Uint = std::uint64_t;
using Float = double;

enum class BasicTypes : std::uint8_t { kChar, kInt, kUint, kFloat };

using TypeInfo = BasicTypes;

}  // namespace semantics
