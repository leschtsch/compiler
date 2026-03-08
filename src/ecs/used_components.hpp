#pragma once

#include <utils/source_position.hpp>

#include <string>

#include "ecs.hpp"

namespace lexer {

struct ErrorStart : public ecs::Component<utils::SourcePosition> {};
struct ErrorStop : public ecs::Component<utils::SourcePosition> {};
struct ErrorMessage : public ecs::Component<std::string> {};

struct StrValue : public ecs::Component<std::string> {};

}  // namespace lexer
