include(FetchContent)
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    GIT_SHALLOW TRUE)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
# doctest 2.4.11 verlangt cmake_minimum_required(VERSION 3.0); CMake 4 lehnt das ohne
# diese Untergrenze ab. Ältere CMake-Versionen ignorieren die Variable.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(doctest)
unset(CMAKE_POLICY_VERSION_MINIMUM)

add_executable(uhrwerk_tests
    test_main.cpp
    test_fixed.cpp
    test_rng.cpp
    test_date.cpp
    test_script.cpp
    test_world.cpp
    test_loader.cpp
    test_simulation.cpp
)
target_link_libraries(uhrwerk_tests PRIVATE uhrwerk::core doctest::doctest uhrwerk_warnings)
target_compile_definitions(uhrwerk_tests PRIVATE
    UHRWERK_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/daten"
    UHRWERK_GAME_DATA_DIR="${PROJECT_SOURCE_DIR}/data")

add_test(NAME uhrwerk_tests COMMAND uhrwerk_tests)

if(UHRWERK_BUILD_CLI)
    # Integration: echte Spieldaten müssen fehlerfrei laden und deterministisch laufen.
    add_test(NAME daten_pruefen
        COMMAND uhrwerk-cli pruefen --daten ${PROJECT_SOURCE_DIR}/data)
    add_test(NAME determinismus_50_jahre
        COMMAND uhrwerk-cli determinismus --daten ${PROJECT_SOURCE_DIR}/data --jahre 50)
endif()
