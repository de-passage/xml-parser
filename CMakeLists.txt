cmake_minimum_required(VERSION 3.28)

project(parse-xml CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SRC_FILES char_stream.cpp)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
list(TRANSFORM SRC_FILES PREPEND ${SRC_DIR}/)

add_library(parser ${SRC_FILES})
target_include_directories(parser PUBLIC ${SRC_DIR}/)

add_library(xml src/xml.cpp)
target_include_directories(xml PUBLIC ${SRC_DIR}/)
target_link_libraries(xml PUBLIC parser)

macro(char_stream_example name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PUBLIC parser)
endmacro()
macro(xml_example name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PUBLIC xml)
endmacro()

char_stream_example(print tests/print_file.cpp)
char_stream_example(words tests/extract_words.cpp)
char_stream_example(strings tests/only_strings.cpp)
xml_example(print_xml tests/xml.cpp)
xml_example(online_xml tests/online_xml.cpp)
