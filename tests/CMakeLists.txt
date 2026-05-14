SET(
  CMAKE_RUNTIME_OUTPUT_DIRECTORY
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests
)

FIND_PACKAGE(GTest QUIET)

IF(GTest_FOUND)
  MESSAGE(INFO "Found gtest")
ELSE()
  MESSAGE(WARNING "GTest not found, using FetchContent")
  INCLUDE(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
  )
  FetchContent_MakeAvailable(googletest)
ENDIF()

IF(GTest_FOUND)
  SET(GTEST_LIBS GTest::gtest GTest::gmock)
ELSE()
  SET(GTEST_LIBS gtest gmock)
ENDIF()

ADD_SUBDIRECTORY(ecs)
ADD_SUBDIRECTORY(utils)
ADD_SUBDIRECTORY(lexer)
