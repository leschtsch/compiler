#pragma once

#include <lexer/lexer.hpp>

#include <vector>

#include "ast.hpp"

namespace parser {

auto Parse(std::vector<lexer::Lexer::TokenVariant> input) -> AstNode;

};
