#pragma once

#include <vector>

struct AstNode {
  std::vector<struct AstNode> children;
};
