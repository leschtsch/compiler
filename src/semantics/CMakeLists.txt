ADD_LIBRARY(build_symtab STATIC build_symtab.cpp)
TARGET_LINK_LIBRARIES(build_symtab PRIVATE project_options)

ADD_LIBRARY(interpreter STATIC interpreter.cpp)
TARGET_LINK_LIBRARIES(interpreter PRIVATE build_symtab project_options)

