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

  std::vector<lexerv2::Token> vec_input;
  lexerv2::Lexer lex(input);

  do {
    vec_input.push_back(lex.NextToken());
  } while (vec_input.back().GetType() != lexerv2::TokenType::kEofToken);

  auto ast = parserv2::Parse(vec_input);
  parserv2::DumpAst(std::cout, ast);
}
