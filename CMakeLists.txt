cmake_minimum_required(VERSION 3.20)
project(EnViDict LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

include(${CMAKE_SOURCE_DIR}/cmake/StaticRuntime.cmake)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc)
    add_compile_definitions(
        UNICODE _UNICODE
        WIN32_LEAN_AND_MEAN NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_CONFIGURATION_TYPES)
        add_compile_options($<$<CONFIG:Release>:/O2> $<$<CONFIG:Release>:/GL>)
        add_link_options($<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF> $<$<CONFIG:Release>:/LTCG>)
    endif()
endif()

# --- SQLite (built lean) ---
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
target_compile_definitions(sqlite3 PUBLIC
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_FTS5
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_AUTHORIZATION
    SQLITE_OMIT_PROGRESS_CALLBACK
    SQLITE_OMIT_SHARED_CACHE
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_DQS=0
)
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /w)
endif()

# --- EnViDict executable ---
set(ENVIDICT_SOURCES
    src/main.cpp
    src/App.cpp
    src/TrayIcon.cpp
    src/SelectionWatcher.cpp
    src/UiaEventHandlers.cpp
    src/BubbleWindow.cpp
    src/PopupWindow.cpp
    src/Renderer.cpp
    src/Dictionary.cpp
    src/Normalize.cpp
    src/Lemmatize.cpp
    src/Settings.cpp
    src/SettingsWindow.cpp
    src/Startup.cpp
    src/Tts.cpp
    src/Clipboard.cpp
    src/Hook.cpp
    src/DpiUtil.cpp
    resources/app.rc
)

add_executable(EnViDict WIN32 ${ENVIDICT_SOURCES})
target_include_directories(EnViDict PRIVATE src)
target_link_libraries(EnViDict PRIVATE
    sqlite3
    user32 gdi32 shell32 advapi32 ole32 oleaut32 uuid
    uiautomationcore
    d2d1 dwrite windowscodecs dwmapi
    sapi
    comctl32
    shlwapi
)
set_target_properties(EnViDict PROPERTIES OUTPUT_NAME "EnViDict")
# The manifest is embedded via resources/app.rc, so disable MSVC's
# auto-generated manifest to avoid duplication.
if(MSVC)
    target_link_options(EnViDict PRIVATE /MANIFEST:NO)
endif()

# --- Dictionary build target ---
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/data/dictionary.db
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/build_dictionary.py
            --output ${CMAKE_SOURCE_DIR}/data/dictionary.db
        DEPENDS
            ${CMAKE_SOURCE_DIR}/tools/build_dictionary.py
            ${CMAKE_SOURCE_DIR}/data/viet_en_dictionary.json
        COMMENT "Building dictionary.db from dictionary source"
        VERBATIM
    )
    add_custom_target(dictionary_db ALL
        DEPENDS ${CMAKE_SOURCE_DIR}/data/dictionary.db)
    add_dependencies(EnViDict dictionary_db)
endif()

# --- install ---
install(TARGETS EnViDict RUNTIME DESTINATION .)
install(FILES ${CMAKE_SOURCE_DIR}/data/dictionary.db DESTINATION . OPTIONAL)
