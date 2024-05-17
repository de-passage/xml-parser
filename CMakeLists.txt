cmake_minimum_required(VERSION 3.29)

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

add_executable(print tests/print_file.cpp)
target_link_libraries(print PRIVATE parser)
add_executable(words tests/extract_words.cpp)
target_link_libraries(words PRIVATE parser)
add_executable(strings tests/only_strings.cpp)
target_link_libraries(strings PRIVATE parser)
add_executable(print_xml tests/xml.cpp)
target_link_libraries(print_xml PRIVATE xml)
