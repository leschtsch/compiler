#include <parser/dump_ast.hpp>
#include <parser/parser.hpp>

#include <iostream>
#include <string>

auto main() -> int {
  std::string input;

  while (!std::cin.eof()) {
    std::string tmp;
    std::getline(std::cin, tmp);
    input += tmp + "\n";
  }

  auto ast = parser::Parse(input);
  parser::DumpAst(std::cout, ast);
}
