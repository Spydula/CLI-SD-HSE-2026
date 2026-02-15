cmake_minimum_required(VERSION 3.20)
project(mini_shell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_STRICT "Enable strict compilation flags" OFF)
option(ENABLE_SANITIZERS "Enable Address/Undefined sanitizers" OFF)
option(BUILD_TESTING "Build unit tests" ON)

add_library(shell_lib src/shell.cpp)
target_include_directories(shell_lib PUBLIC include)

add_executable(mini_shell src/main.cpp)
target_link_libraries(mini_shell PRIVATE shell_lib)

if (ENABLE_STRICT)
  if (MSVC)
    set(MY_COMPILE_FLAGS /W4 /WX)
  else()
    set(MY_COMPILE_FLAGS 
      -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion 
      -Wsign-conversion -Wold-style-cast -Wnull-dereference -Wformat=2
    )
  endif()
  target_compile_options(shell_lib PRIVATE ${MY_COMPILE_FLAGS})
  target_compile_options(mini_shell PRIVATE ${MY_COMPILE_FLAGS})
endif()

if (ENABLE_SANITIZERS AND NOT MSVC)
  set(SAN_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_compile_options(shell_lib PUBLIC ${SAN_FLAGS})
  target_link_options(shell_lib PUBLIC ${SAN_FLAGS})
endif()

if(BUILD_TESTING)
  enable_testing()

  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_executable(unit_tests
      tests/shell_tests.cpp
      tests/expansion_tests.cpp
      tests/pipeline_tests.cpp
  )
  target_link_libraries(unit_tests PRIVATE shell_lib GTest::gtest_main)
  
  include(GoogleTest)
  gtest_discover_tests(unit_tests)
endif()