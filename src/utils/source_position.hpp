#pragma once

#include <cstddef>

namespace utils {

struct SourcePosition {
  std::size_t index{0};
  std::size_t line{0};
  std::size_t col{0};
};

}  // namespace utils
