cmake_minimum_required(VERSION 3.20)
project(mini_shell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(mini_shell
  src/main.cpp
  src/shell.cpp
)

target_include_directories(mini_shell PRIVATE src)

option(ENABLE_STRICT "Enable strict compilation flags" OFF)
option(ENABLE_SANITIZERS "Enable Address/Undefined sanitizers" OFF)

if (ENABLE_STRICT)
  if (MSVC)
    target_compile_options(mini_shell PRIVATE /W4 /WX)
  else()
    target_compile_options(mini_shell PRIVATE
      -Wall -Wextra -Wpedantic
      -Werror
      -Wshadow
      -Wconversion
      -Wsign-conversion
      -Wold-style-cast
      -Wcast-align
      -Woverloaded-virtual
      -Wnon-virtual-dtor
      -Wnull-dereference
      -Wdouble-promotion
      -Wformat=2
      -Wimplicit-fallthrough
    )
  endif()
else()
  if (MSVC)
    target_compile_options(mini_shell PRIVATE /W4)
  else()
    target_compile_options(mini_shell PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

if (ENABLE_SANITIZERS)
  if (MSVC)
    message(WARNING "Sanitizers are not configured for MSVC in this template.")
  else()
    target_compile_options(mini_shell PRIVATE
      -fsanitize=address,undefined
      -fno-omit-frame-pointer
      -fno-sanitize-recover=all
    )
    target_link_options(mini_shell PRIVATE
      -fsanitize=address,undefined
    )
  endif()
endif()