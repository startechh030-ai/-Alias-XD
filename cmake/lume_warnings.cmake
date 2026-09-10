# One warning policy for the whole tree. CI turns it into an error; humans do not.
function(lume_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(LUME_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
    # /WT<seconds> would be noise here; keep the suppressions we actually need.
    target_compile_options(${target} PRIVATE /wd4324)  # padding structure
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion
      -Wnon-virtual-dtor -Wcast-align -Woverloaded-virtual -Wnull-dereference
      -Wold-style-cast -Wdouble-promotion -Wformat=2)
    if(LUME_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()
