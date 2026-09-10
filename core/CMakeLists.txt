# lume-core: the project system. No dependencies beyond the C++17 standard library,
# on purpose - this is what CI can compile and unit-test in seconds on three platforms.

add_library(lume_core STATIC
  src/json.cpp
  src/fs.cpp
  src/log.cpp
  src/settings.cpp
  src/project.cpp
  src/templates.cpp
)
add_library(lume::core ALIAS lume_core)

target_include_directories(lume_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(lume_core PUBLIC cxx_std_17)
lume_apply_warnings(lume_core)
lume_add_platform_flags(lume_core)

# GCC < 9 / Clang < 9 keep <filesystem> in a separate library.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
  target_link_libraries(lume_core PUBLIC stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
  target_link_libraries(lume_core PUBLIC c++fs)
endif()

# The engine version stamp the hub compares against project `engineMin`.
target_compile_definitions(lume_core PUBLIC LUME_VERSION="0.1.0")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
  src/json.cpp src/fs.cpp src/log.cpp src/settings.cpp src/project.cpp src/templates.cpp
  include/lume/json.hpp include/lume/fs.hpp include/lume/log.hpp include/lume/settings.hpp
  include/lume/project.hpp include/lume/templates.hpp)
