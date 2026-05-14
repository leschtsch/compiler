ADD_LIBRARY(lexer STATIC lexer.cpp)
TARGET_LINK_LIBRARIES(lexer PRIVATE project_options)
