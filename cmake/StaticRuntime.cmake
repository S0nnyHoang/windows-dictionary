# Force static CRT (/MT, /MTd) for all targets so the final exe needs no
# vcruntime DLLs and link-time dead-code removal can shrink it further.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
