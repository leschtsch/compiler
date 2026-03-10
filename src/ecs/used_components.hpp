#pragma once

#include <utils/source_position.hpp>

#include <cstdint>
#include <string>

#include "ecs.hpp"

namespace lexer {

struct TokenStart : public ecs::Component<utils::SourcePosition> {};
struct TokenStop : public ecs::Component<utils::SourcePosition> {};

struct ErrorMessage : public ecs::Component<std::string> {};

struct StrValue : public ecs::Component<std::string> {};
struct IntValue : public ecs::Component<std::uintmax_t> {};
struct FloatValue : public ecs::Component<long double> {};

struct IdName : public ecs::Component<std::string> {};

}  // namespace lexer
