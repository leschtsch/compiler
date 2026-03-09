#include <lexer/lexer.hpp>
#include <lexer/tokens.hpp>

#include <iostream>
#include <string>
#include <variant>

auto main() -> int {
  std::string input;

  while (!std::cin.eof()) {
    std::string tmp;
    std::getline(std::cin, tmp);
    input += tmp+ "\n";
  }

  lexer::Lexer lexer(input);

  while (true) {
    auto tok = lexer.NextToken();
    if (std::holds_alternative<lexer::tokens::EofToken>(tok)) {
      std::cout << "EOF\n";
      break;
    }
    std::cout << tok.index() << "\n";
  }
}
