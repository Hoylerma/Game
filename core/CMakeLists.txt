# Uhrwerk – deterministischer Simulationskern. Keine Abhängigkeit zu Godot.
add_library(uhrwerk_core STATIC
    src/fixed.cpp
    src/date.cpp
    src/script.cpp
    src/world.cpp
    src/loader.cpp
    src/simulation.cpp
    src/parallel.cpp
)
add_library(uhrwerk::core ALIAS uhrwerk_core)

target_include_directories(uhrwerk_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(uhrwerk_core PUBLIC cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(uhrwerk_core PUBLIC Threads::Threads PRIVATE uhrwerk_warnings)
