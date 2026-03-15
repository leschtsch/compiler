ADD_LIBRARY(parser STATIC parser.cpp)
TARGET_LINK_LIBRARIES(parser PRIVATE lexer)

ADD_LIBRARY(dump_ast STATIC dump_ast.cpp)
