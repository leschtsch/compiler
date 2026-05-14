#pragma once

#include <lexer/tokens.hpp>

#include <vector>

#include "ast.hpp"

namespace parser {

// will hold Program*
[[nodiscard]] nodes::NodesVariant Parse(std::vector<lexer::Token> input);

};
