#pragma once

#include <lexer/tokens.hpp>

#include <vector>

#include "ast.hpp"

namespace parserv2 {

// will hold Program*
[[nodiscard]] nodes::NodesVariant Parse(std::vector<lexerv2::Token> input);

};
