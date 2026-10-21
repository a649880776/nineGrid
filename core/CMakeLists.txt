cmake_minimum_required(VERSION 3.16)
project(ninegrid_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ninegrid_core STATIC
  src/engine.cpp
  src/lexicon.cpp
  src/syllable_table.cpp
  src/t9_decoder.cpp
  src/utf8.cpp
  src/user_model.cpp
  src/word_decoder.cpp
)
target_include_directories(ninegrid_core
  PUBLIC include
  PRIVATE src
)
if (MSVC)
  target_compile_options(ninegrid_core PRIVATE /W4)
else()
  target_compile_options(ninegrid_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(ninegrid_cli cli/main.cpp)
target_link_libraries(ninegrid_cli PRIVATE ninegrid_core)

add_executable(ninegrid_tests tests/test_core.cpp)
target_link_libraries(ninegrid_tests PRIVATE ninegrid_core)

enable_testing()
add_test(NAME core_tests
         COMMAND ninegrid_tests ${CMAKE_CURRENT_SOURCE_DIR}/data/sample_dict.tsv)
