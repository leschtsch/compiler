ADD_LIBRARY(parser STATIC parser.cpp)
TARGET_LINK_LIBRARIES(parser PRIVATE project_options)

ADD_LIBRARY(dump_ast STATIC dump_ast.cpp)
TARGET_LINK_LIBRARIES(dump_ast PRIVATE project_options)
