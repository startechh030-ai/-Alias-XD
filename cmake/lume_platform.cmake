# Platform detection + per-platform compile flags for every Lume target.
# Android is a first-class target here, not an afterthought: it is the only one
# that needs a hard API floor for GL ES 3.1 / Vulkan.

if(ANDROID)
  set(LUME_PLATFORM "android")
elseif(WIN32)
  set(LUME_PLATFORM "windows")
elseif(APPLE)
  # Deliberately unsupported: no macOS, no iOS. Kept detectable so a stray
  # CMakeUserPresets entry fails loudly instead of half-building.
  set(LUME_PLATFORM "macos")
elseif(UNIX)
  set(LUME_PLATFORM "linux")
else()
  set(LUME_PLATFORM "unknown")
endif()

if(LUME_PLATFORM STREQUAL "macos")
  message(FATAL_ERROR "Lume targets Android, Windows and Linux only. macOS is not supported.")
endif()

function(lume_add_platform_flags target)
  if(LUME_PLATFORM STREQUAL "windows")
    # utf-8 source + exec, WIN32_LEAN_AND_MEAN, and no windows.h macro pollution.
    target_compile_definitions(${target} PRIVATE
      UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
      _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
      target_compile_options(${target} PRIVATE /utf-8 /Zc:__cplusplus /EHsc /MP)
    endif()
  elseif(LUME_PLATFORM STREQUAL "android")
    target_compile_definitions(${target} PRIVATE LUME_ANDROID=1)
    target_compile_options(${target} PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(${target} PRIVATE -Wl,--gc-sections -Wl,-z,max-page-size=16384)
  elseif(LUME_PLATFORM STREQUAL "linux")
    target_compile_options(${target} PRIVATE -ffunction-sections -fdata-sections)
  endif()
endfunction()
