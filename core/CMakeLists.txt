# Uhrwerk – deterministischer Simulationskern. Keine Abhängigkeit zu Godot.
add_library(uhrwerk_core STATIC
    src/fixed.cpp
    src/date.cpp
    src/script.cpp
    src/world.cpp
    src/loader.cpp
    src/simulation.cpp
    src/parallel.cpp
    src/politics.cpp
    src/population.cpp
    src/economy.cpp
    src/lasttest.cpp
    src/balance.cpp
    src/savegame.cpp
)
add_library(uhrwerk::core ALIAS uhrwerk_core)

target_include_directories(uhrwerk_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(uhrwerk_core PUBLIC cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(uhrwerk_core PUBLIC Threads::Threads PRIVATE uhrwerk_warnings)
# Spielversion in den Metadaten der Spielstände (seit M6).
target_compile_definitions(uhrwerk_core PRIVATE UHRWERK_VERSION="${PROJECT_VERSION}")

# Kompression der Spielstände (seit M6, docs/technik/speicherstaende.md): zstd 1.5.6 (BSD-Lizenz) als
# statische Bibliothek, ohne Programme, Tests und eigene Threads. Feste Version mit Prüfsumme.
include(FetchContent)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zstd
    URL https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz
    URL_HASH SHA256=8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1
    SOURCE_SUBDIR build/cmake
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)
FetchContent_MakeAvailable(zstd)
target_link_libraries(uhrwerk_core PRIVATE libzstd_static)
target_include_directories(uhrwerk_core SYSTEM PRIVATE ${zstd_SOURCE_DIR}/lib)
