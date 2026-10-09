# GDExtension-Brücke zwischen Uhrwerk und Godot 4.4.
set(GODOTCPP_TARGET "template_debug" CACHE STRING "godot-cpp Ziel: template_debug, template_release oder editor")
# SYSTEM: Warnungen aus den godot-cpp-Headern nicht in eigenen Übersetzungseinheiten melden.
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/godot-cpp ${CMAKE_BINARY_DIR}/godot-cpp EXCLUDE_FROM_ALL SYSTEM)

add_library(uhrwerk_bridge SHARED
    src/register_types.cpp
    src/uhrwerk_simulation.cpp
)
target_link_libraries(uhrwerk_bridge PRIVATE uhrwerk::core godot::cpp uhrwerk_warnings)

# godot-cpp legt unter MSVC beim Einbinden die statische Laufzeit (/MT) fest. Der Kern ist
# dann schon angelegt und hätte die Standard-Laufzeit (/MD) – das Linken der DLL scheitert
# mit LNK2038. Deshalb übernimmt der Kern die Laufzeit von godot-cpp.
if(MSVC)
    get_target_property(_uhrwerk_runtime godot-cpp MSVC_RUNTIME_LIBRARY)
    if(NOT _uhrwerk_runtime AND DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
        set(_uhrwerk_runtime "${CMAKE_MSVC_RUNTIME_LIBRARY}")
    endif()
    if(_uhrwerk_runtime)
        set_property(TARGET uhrwerk_core PROPERTY MSVC_RUNTIME_LIBRARY "${_uhrwerk_runtime}")
    endif()
endif()

# Ausgabe direkt in das Godot-Projekt; $<1:…> verhindert Unterordner je Konfiguration.
set(UHRWERK_GAME_BIN "${PROJECT_SOURCE_DIR}/game/bin")
set_target_properties(uhrwerk_bridge PROPERTIES
    OUTPUT_NAME "uhrwerk_bridge"
    LIBRARY_OUTPUT_DIRECTORY "$<1:${UHRWERK_GAME_BIN}>"
    RUNTIME_OUTPUT_DIRECTORY "$<1:${UHRWERK_GAME_BIN}>"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

# Nur uhrwerk_library_init exportieren, nicht die Symbole des statisch gelinkten Kerns.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(uhrwerk_bridge PRIVATE "LINKER:--exclude-libs,ALL")
endif()
