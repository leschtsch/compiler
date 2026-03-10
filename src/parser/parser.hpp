#pragma once

#include <string>

#include "ast.hpp"

namespace parser {

auto Parse(const std::string& input) -> AstNode;

};
