# Unit tests. Each is a plain executable that returns non-zero on failure, which is all
# ctest needs. No network, no GPU, no install step - they run on a bare runner in seconds.

function(lume_add_test name)
  cmake_parse_arguments(LAT "" "" "SOURCES;LIBS" ${ARGN})
  add_executable(${name} ${LAT_SOURCES})
  target_link_libraries(${name} PRIVATE ${LAT_LIBS})
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(${name} PRIVATE cxx_std_17)
  # Test code is allowed to be sloppier than the shipped code: no -Werror here.
  if(NOT MSVC)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wno-old-style-cast)
  endif()
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES
    TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endfunction()

lume_add_test(test_project
  SOURCES test_project.cpp
  LIBS lume::core)

lume_add_test(test_hub_state
  SOURCES test_hub_state.cpp
  LIBS lume_hub_logic)

# The hub binary itself is smoke-tested from ci.yml (it creates a project, prints the hub,
# re-reads it), because that path crosses main.cpp, which no unit test links.
