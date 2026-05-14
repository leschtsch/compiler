ADD_SUBDIRECTORY(lexer)
ADD_SUBDIRECTORY(parser)

ADD_EXECUTABLE(main main.cpp)
TARGET_LINK_LIBRARIES(main PRIVATE project_options lexer parser dump_ast)
