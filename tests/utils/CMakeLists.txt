ADD_EXECUTABLE(type_tuple type_tuple.cpp)
ADD_TEST(
  NAME type_tuple
  COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/type_tuple
)

