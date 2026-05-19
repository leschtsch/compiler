ADD_LIBRARY(parser STATIC parser.cpp)
TARGET_LINK_LIBRARIES(parser PRIVATE project_options)
