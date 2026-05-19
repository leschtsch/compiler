ADD_SUBDIRECTORY(lexer)
ADD_SUBDIRECTORY(parser)
ADD_SUBDIRECTORY(semantics)
ADD_SUBDIRECTORY(misc)

ADD_EXECUTABLE(main main.cpp)
TARGET_LINK_LIBRARIES(main PRIVATE project_options lexer parser dump_ast build_symtab)
