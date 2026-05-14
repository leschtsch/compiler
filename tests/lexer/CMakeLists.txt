ADD_EXECUTABLE(test_lexer lexer.cpp)
TARGET_LINK_LIBRARIES(test_lexer PRIVATE project_options lexer ${GTEST_LIBS})
ADD_TEST(
  NAME lexer
  COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_lexer
)
