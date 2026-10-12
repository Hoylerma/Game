cmake_minimum_required(VERSION 3.25)

project(Einigkeit
    VERSION 0.1.0
    DESCRIPTION "EINIGKEIT – Grand Strategy über deutsche Geschichte von 1648 bis heute"
    LANGUAGES C CXX) # C nur für zstd (Kompression der Spielstände, seit M6)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Der Kern wird statisch in die GDExtension gelinkt und muss daher positionsunabhängig sein.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(UHRWERK_BUILD_CLI "Kommandozeile uhrwerk-cli bauen" ON)
option(UHRWERK_BUILD_TESTS "Tests bauen" ON)
option(UHRWERK_BUILD_BRIDGE "GDExtension für Godot bauen (benötigt third_party/godot-cpp)" OFF)
option(UHRWERK_WARNINGS_AS_ERRORS "Compiler-Warnungen als Fehler behandeln" OFF)

# Gemeinsame Warnungen für eigene Ziele (nicht für Fremdbibliotheken).
add_library(uhrwerk_warnings INTERFACE)
if(MSVC)
    target_compile_options(uhrwerk_warnings INTERFACE /W4 /permissive- /utf-8)
    if(UHRWERK_WARNINGS_AS_ERRORS)
        target_compile_options(uhrwerk_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(uhrwerk_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor)
    if(UHRWERK_WARNINGS_AS_ERRORS)
        target_compile_options(uhrwerk_warnings INTERFACE -Werror)
    endif()
endif()

add_subdirectory(core)

if(UHRWERK_BUILD_CLI)
    add_subdirectory(cli)
endif()

if(UHRWERK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(UHRWERK_BUILD_BRIDGE)
    add_subdirectory(bridge)
endif()
