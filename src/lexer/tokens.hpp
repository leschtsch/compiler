#pragma once

#include <ecs/ecs.hpp>
#include <utils/type_tuple.hpp>

namespace lexer::tokens {

//===========================V=MISCELLANEOUS=TOKENS=V===============================================
struct BaseToken : ecs::Entity {};

struct ErrorToken : public BaseToken {};
struct EofToken : public BaseToken {};
struct NopToken : public BaseToken {};

using MiscellaneousTuple = utils::TypeTuple<ErrorToken, EofToken, NopToken, BaseToken>;
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
  struct Keyword##name : public Keyword {};
#include <language_data/keywords.dat>
#undef KEYWORD

#define KEYWORD(name) Keyword##name,

using KeywordsTuple = utils::TypeTuple<
#include <language_data/keywords.dat>
    Keyword>;

#undef KEYWORD
//===========================^=KEYWORDS=^===========================================================

//===========================V=ALL=TOKENS=V=========================================================
using AllTokensTuple = utils::Concat<UnOperatorsTuple,
                                     BinOperatorsTuple,
                                     LiteralsTuple,
                                     KeywordsTuple,
                                     MiscellaneousTuple>;
//===========================^=ALL=TOKENS=^=========================================================
}  // namespace lexer::tokens
