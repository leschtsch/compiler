#pragma once

namespace utils {

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};

}  // namespace utils
