cmake_minimum_required(VERSION 3.14)
project(mdcat LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/md4c/md4c.c")
  add_library(md4c STATIC third_party/md4c/md4c.c)
  target_include_directories(md4c PUBLIC third_party/md4c)
  set(MD4C_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/md4c")
else()
  include(FetchContent)
  set(BUILD_MD2HTML_EXECUTABLE OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(md4c GIT_REPOSITORY https://github.com/mity/md4c.git GIT_TAG release-0.5.2)
  FetchContent_MakeAvailable(md4c)
  set(MD4C_INCLUDE_DIR "${md4c_SOURCE_DIR}/src")
endif()

add_library(mdcat_ascii STATIC src/ascii/glyphs.cpp src/ascii/canvas.cpp src/text.cpp)
target_include_directories(mdcat_ascii PUBLIC include)

add_library(mdcat_mermaid STATIC
  src/mermaid/mermaid.cpp src/mermaid/labels.cpp src/mermaid/flowchart.cpp
  src/mermaid/sequence.cpp src/mermaid/pie.cpp)
target_link_libraries(mdcat_mermaid PUBLIC mdcat_ascii)

add_library(mdcat_md STATIC src/md/terminal_renderer.cpp)
target_include_directories(mdcat_md PRIVATE "${MD4C_INCLUDE_DIR}")
target_link_libraries(mdcat_md PUBLIC mdcat_ascii PRIVATE md4c)

add_executable(mdcat app/main.cpp)
target_link_libraries(mdcat PRIVATE mdcat_md mdcat_mermaid)
if(MSVC)
  foreach(t mdcat mdcat_ascii mdcat_mermaid mdcat_md)
    target_compile_options(${t} PRIVATE /utf-8 /EHsc)
  endforeach()
endif()
install(TARGETS mdcat RUNTIME DESTINATION bin)
