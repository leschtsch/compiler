/**
 * @file tokens.hpp
 *
 * @note although lexer can't distinguish between unary
 * and binary operators, such as unary and binary minus, or multiplication and dereference
 * they're declared twice.
 * Lexer should prefer binary, parser may fix that.
 */
#pragma once

#include <ecs/ecs.hpp>
#include <utils/type_tuple.hpp>

namespace lexer::tokens {

//===========================V=MISCELLANEOUS=TOKENS=V===============================================
struct BaseToken : ecs::Entity {};

struct ErrorToken : public BaseToken {};
struct EofToken : public BaseToken {};
struct IdToken : public BaseToken {};

using MiscellaneousTuple =
    utils::TypeTuple<ErrorToken, EofToken, IdToken, BaseToken>;
//===========================^=MISCELLANEOUS=TOKENS=^===============================================

//===========================V=UNARY=OPERATORS=V====================================================
struct UnOperator : public BaseToken {};

#define UN_OP(name, x) \
  struct Un##name : public UnOperator {};
#include <language_data/un_ops.dat>
#undef UN_OP

#define UN_OP(name, x) Un##name,
using UnOperatorsTuple = utils::TypeTuple<
#include <language_data/un_ops.dat>
    UnOperator>;
#undef UN_OP
//===========================^=UNARY=OPERATORS=^====================================================
//===========================V=UNARY=OPERATORS=V====================================================
struct BinOperator : public BaseToken {};

#define BIN_OP(name, x) \
  struct Bin##name : public BinOperator {};
#include <language_data/bin_ops.dat>
#undef BIN_OP

#define BIN_OP(name, x) Bin##name,

using BinOperatorsTuple = utils::TypeTuple<
#include <language_data/bin_ops.dat>
    BinOperator>;

#undef BIN_OP
//===========================^=UNARY=OPERATORS=^====================================================

//===========================V=LITERALS=V===========================================================
struct Literal : public BaseToken {};

struct Int : public Literal {};
struct Float : public Literal {};
struct String : public Literal {};

using LiteralsTuple = utils::TypeTuple<Int, Float, String, Literal>;
//===========================^=LITERALS=^===========================================================

//===========================V=KEYWORDS=V===========================================================
struct Keyword : public BaseToken {};

#define KEYWORD(name) \
  struct Keyword_##name : public Keyword {};
#include <language_data/keywords.dat>
#undef KEYWORD

#define KEYWORD(name) Keyword_##name,

using KeywordsTuple = utils::TypeTuple<
#include <language_data/keywords.dat>
    Keyword>;

#undef KEYWORD
//===========================^=KEYWORDS=^===========================================================

//===========================V=SYNTAX=TOKENS=V======================================================

struct SyntaxToken : public BaseToken {};

#define SYNT_TOKEN(name, x) \
  struct Syntax##name : public SyntaxToken {};
#include <language_data/synt_tokens.dat>
#undef SYNT_TOKEN

#define SYNT_TOKEN(name, x) Syntax##name,

using SyntaxTokensTuple = utils::TypeTuple<
#include <language_data/synt_tokens.dat>
    SyntaxToken>;

#undef SYNT_TOKEN
//===========================^=SYNTAX=TOKENS=^======================================================

//===========================V=ALL=TOKENS=V=========================================================
using AllTokensTuple = utils::Concat<UnOperatorsTuple,
                                     BinOperatorsTuple,
                                     LiteralsTuple,
                                     KeywordsTuple,
                                     SyntaxTokensTuple,
                                     MiscellaneousTuple>;
//===========================^=ALL=TOKENS=^=========================================================
}  // namespace lexer::tokens
