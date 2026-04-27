#include <parser/dump_ast.hpp>
#include <parser/parser.hpp>

#include <iostream>
#include <string>
#include <vector>

#include "lexer/lexer.hpp"
#include "lexer/tokens.hpp"

auto main() -> int {
  std::string input;

  while (!std::cin.eof()) {
    std::string tmp;
    std::getline(std::cin, tmp);
    input += tmp + "\n";
  }

  std::vector<lexer::Token> vec_input;
  lexer::Lexer lex(input);

  do {
    vec_input.push_back(lex.NextToken());
  } while (vec_input.back().GetType() != lexer::TokenType::kEofToken);

  auto ast = parser::Parse(vec_input);
  parser::DumpAst(std::cout, ast);
}
