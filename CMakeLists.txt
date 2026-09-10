cmake_minimum_required(VERSION 3.24)

project(lume
  VERSION 0.1.0
  DESCRIPTION "Lume - a lightweight 3D creation tool (hub, renderer, geometry)"
  LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options. Default everything heavy OFF: the core must configure on a bare
# runner in seconds. Steps 2+ flip these on.
# ---------------------------------------------------------------------------
option(LUME_BUILD_HUB        "Build the hub / project launcher"      ON)
option(LUME_BUILD_TESTS      "Build unit tests (ctest)"              ON)
option(LUME_WARNINGS_AS_ERRORS "Treat warnings as errors (CI: ON)"   OFF)

# --- Step 2: renderer (Filament). These exist now so the seam is visible. ---
option(LUME_ENABLE_FILAMENT  "Link Google Filament (step 2)"         OFF)
option(LUME_ENABLE_IMGUI     "Build the immediate-mode hub UI"       OFF)
set(LUME_FILAMENT_DIST_DIR "" CACHE PATH "Unpacked Filament release (releases/cmake/{linux,windows})")
set(LUME_IMGUI_DIR "" CACHE PATH "Path to vendored Dear ImGui root")

# ---------------------------------------------------------------------------
# Global compiler behaviour
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(lume_platform)   # lume_add_platform_flags(), platform detection
include(lume_warnings)   # lume_apply_warnings(<target>)

if(LUME_ENABLE_FILAMENT AND NOT LUME_FILAMENT_DIST_DIR)
  message(FATAL_ERROR
    "LUME_ENABLE_FILAMENT=ON requires LUME_FILAMENT_DIST_DIR pointing at an unpacked "
    "Filament release (that is step 2; see docs/roadmap.md and .github/workflows/).")
endif()

add_subdirectory(core)

if(LUME_BUILD_HUB)
  add_subdirectory(hub)
endif()

if(LUME_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
  enable_testing()
  add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Informational summary - useful in CI logs when a build config is wrong.
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "lume ${PROJECT_VERSION}  platform=${LUME_PLATFORM}  system=${CMAKE_SYSTEM_NAME}")
message(STATUS "  hub            : ${LUME_BUILD_HUB}")
message(STATUS "  filament (s2)  : ${LUME_ENABLE_FILAMENT}")
message(STATUS "  imgui          : ${LUME_ENABLE_IMGUI}")
message(STATUS "  tests          : ${LUME_BUILD_TESTS}")
message(STATUS "")
