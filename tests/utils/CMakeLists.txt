ADD_EXECUTABLE(type_tuple type_tuple.cpp)
TARGET_LINK_LIBRARIES(type_tuple PRIVATE project_options ${GTEST_LIBS})
ADD_TEST(
  NAME type_tuple
  COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/type_tuple
)

