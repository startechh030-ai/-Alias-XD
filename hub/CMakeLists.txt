# The hub. Two targets, because the logic has to be testable where no UI toolkit exists:
#
#   lume_hub_logic   HubState: tabs, search, sorting, card building. No ImGui, no window.
#   lume-hub         the executable: text hub today, Filament window in step 2.

add_library(lume_hub_logic STATIC
  src/hub_state.cpp
  src/hub_ui.cpp
)
add_library(lume::hub_logic ALIAS lume_hub_logic)
target_link_libraries(lume_hub_logic PUBLIC lume::core)
target_compile_features(lume_hub_logic PUBLIC cxx_std_17)
lume_apply_warnings(lume_hub_logic)
lume_add_platform_flags(lume_hub_logic)

add_executable(lume-hub
  src/main.cpp
)
target_link_libraries(lume-hub PRIVATE lume_hub_logic)
lume_apply_warnings(lume-hub)
lume_add_platform_flags(lume-hub)

# Lets a build tree find templates/ without an install step.
target_compile_definitions(lume-hub PRIVATE LUME_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Android keeps the hub out of the APK for now: it is a desktop-first surface. The Android
# target builds lume_core + the logic lib only, which is also what android.yml checks.
if(ANDROID)
  set_target_properties(lume-hub PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

# ---- optional: Dear ImGui, which paints the mockup ----------------------------------
if(LUME_ENABLE_IMGUI)
  if(NOT LUME_IMGUI_DIR OR NOT EXISTS "${LUME_IMGUI_DIR}/imgui.h")
    message(FATAL_ERROR
      "LUME_ENABLE_IMGUI=ON but LUME_IMGUI_DIR (\"${LUME_IMGUI_DIR}\") has no imgui.h.\n"
      "Drop dear imgui into external/imgui and point the option at it (see external/README.md).")
  endif()
  file(GLOB LUME_IMGUI_SOURCES CONFIGURE_DEPENDS "${LUME_IMGUI_DIR}/imgui*.cpp")
  list(FILTER LUME_IMGUI_SOURCES EXCLUDE REGEX "/backends/")   # our backend is Filament's, step 2
  add_library(lume_imgui STATIC ${LUME_IMGUI_SOURCES})
  target_include_directories(lume_imgui PUBLIC "${LUME_IMGUI_DIR}")
  target_compile_features(lume_imgui PUBLIC cxx_std_17)
  if(MSVC)
    target_compile_options(lume_imgui PRIVATE /W0)
  else()
    target_compile_options(lume_imgui PRIVATE -w)
  endif()
  target_link_libraries(lume_hub_logic PUBLIC lume_imgui)
  target_compile_definitions(lume_hub_logic PUBLIC LUME_WITH_IMGUI=1)
endif()

# ---- step 2 seam ----------------------------------------------------------------------
if(LUME_ENABLE_FILAMENT)
  # Deliberately not a stub: step 2 replaces the text hub with a Filament swapchain +
  # imgui-as-texture path. Until that exists, configuring it on is an error, not a warning.
  message(FATAL_ERROR
    "Filament is step 2 of docs/roadmap.md and is not implemented yet. "
    "See docs/02-renderer.md for the intended shape.")
endif()

install(TARGETS lume-hub RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
