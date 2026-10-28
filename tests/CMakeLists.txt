include(FetchContent)
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    GIT_SHALLOW TRUE
    SYSTEM)  # Warnungen aus doctest.h zählen nicht als eigene (/WX, -Werror)
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
    test_parallel.cpp
    test_population.cpp
    test_lasttest.cpp
    test_balance.cpp
    test_savegame.cpp
    test_politics.cpp
    test_customs.cpp
    test_diplomacy.cpp
    test_military.cpp
    test_events.cpp
)
target_link_libraries(uhrwerk_tests PRIVATE uhrwerk::core doctest::doctest uhrwerk_warnings)
target_compile_definitions(uhrwerk_tests PRIVATE
    # Echte Standard-Header statt Vorwärtsdeklarationen in namespace std: Neuere MSVC
    # melden für die Deklaration von std::tuple C5285 (verbotene Spezialisierung).
    DOCTEST_CONFIG_USE_STD_HEADERS
    UHRWERK_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/daten"
    UHRWERK_GAME_DATA_DIR="${PROJECT_SOURCE_DIR}/data")

add_test(NAME uhrwerk_tests COMMAND uhrwerk_tests)

if(UHRWERK_BUILD_CLI)
    # Integration: echte Spieldaten müssen fehlerfrei laden und deterministisch laufen.
    add_test(NAME daten_pruefen
        COMMAND uhrwerk-cli pruefen --daten ${PROJECT_SOURCE_DIR}/data)
    add_test(NAME determinismus_50_jahre
        COMMAND uhrwerk-cli determinismus --daten ${PROJECT_SOURCE_DIR}/data --jahre 50)
    # Lesezeichen 1848 (seit M7): 3.967 Orte mit Daten; im Debug-Build rund 2 s je Spieljahr.
    add_test(NAME daten_pruefen_1848
        COMMAND uhrwerk-cli pruefen --daten ${PROJECT_SOURCE_DIR}/data --szenario voelkerfruehling)
    add_test(NAME determinismus_1848
        COMMAND uhrwerk-cli determinismus --daten ${PROJECT_SOURCE_DIR}/data --szenario voelkerfruehling
                --jahre 2)
    # Lasttest im Kleinen (seit M4): gleiche Prüfsummen mit 3 Threads und mit einem Thread.
    add_test(NAME lasttest_threads
        COMMAND uhrwerk-cli lasttest --daten ${PROJECT_SOURCE_DIR}/data --bloecke 20000 --jahre 1
                --threads 3 --vergleich 1 --speichern ${CMAKE_CURRENT_BINARY_DIR}/lasttest.einigkeit)
    # Balancing-Bericht (seit M5): auf der Fixture mit ihren weiten Zielwerten in Ordnung, mit den
    # Zielwerten der Spieldaten verfehlt. Der volle Bericht läuft im CI-Job "Balancing".
    add_test(NAME bilanz_fixture
        COMMAND uhrwerk-cli bilanz --daten ${CMAKE_CURRENT_SOURCE_DIR}/daten --seeds 3 --jahre 30)
    add_test(NAME bilanz_verstoss
        COMMAND uhrwerk-cli bilanz --daten ${CMAKE_CURRENT_SOURCE_DIR}/daten --seeds 3 --jahre 30
                --ziele ${PROJECT_SOURCE_DIR}/data/common/zielwerte.chron)
    # Mit PASS_REGULAR_EXPRESSION zählt die Ausgabe statt des Exitcodes: Ein Bedienfehler bestünde nicht.
    set_tests_properties(bilanz_verstoss PROPERTIES PASS_REGULAR_EXPRESSION "Verstöße \\([0-9]+, mit ! markiert\\)")
    # Wahllabor (seit M8): Landtag der Fixture mit ihren Wahlzielen; die Nationalversammlung 1848 mit
    # zwei Seeds (die volle Prüfung mit 16 Seeds läuft im CI-Job "Balancing").
    add_test(NAME wahl_fixture
        COMMAND uhrwerk-cli wahl --daten ${CMAKE_CURRENT_SOURCE_DIR}/daten --szenario jahresbeginn_1650 --seeds 3
                --wahlkreise)
    add_test(NAME wahl_1848
        COMMAND uhrwerk-cli wahl --daten ${PROJECT_SOURCE_DIR}/data --szenario voelkerfruehling --seeds 2)
    # Zollunionen 1848 (seit M9, docs/technik/diplomatie.md Abschnitt 7): die Mitglieder von Zollverein und
    # Steuerverein; die übrigen neun Staaten sind je ein eigenes Zollgebiet.
    add_test(NAME zollunionen_1848
        COMMAND uhrwerk-cli info --daten ${PROJECT_SOURCE_DIR}/data --szenario voelkerfruehling)
    set_tests_properties(zollunionen_1848 PROPERTIES PASS_REGULAR_EXPRESSION
        "2 Zollunionen, 9 Staaten für sich\n[^\n]*Zollverein \\(Leitung PRU, Tarif 10 %\\): 26 Staaten[^\n]*\n *ALT BAD BAV BER BWO DES FRA HED HEK HHO HOH HSI LIP LUX NAS PRU REA REJ SAX SCG SMG SRU SSH WAL WEI WUE\n[^\n]*Steuerverein \\(Leitung HAN, Tarif 3 %\\): 3 Staaten[^\n]*\n *HAN OLD SLI\n")

    # Zolllabor (seit M9): ein kurzer Lauf mit einem Wechsel; die volle Prüfung läuft im CI-Job "Balancing".
    add_test(NAME zoll_1848
        COMMAND uhrwerk-cli zoll --daten ${PROJECT_SOURCE_DIR}/data --szenario voelkerfruehling --seeds 1
                --jahre 1 --beitritt SLI:zollverein)

    # Kriegslabor (seit M10, docs/technik/militaer.md Abschnitt 6.3): drei Kriege auf der Fixture mit
    # Militärdaten (tests/daten_krieg über tests/daten) gegen ihre Kriegsziele über 16 Seeds; 1848 ein
    # kurzer Lauf, die volle Prüfung läuft im CI-Job "Balancing".
    set(krieg_daten ${CMAKE_CURRENT_BINARY_DIR}/krieg_daten)
    add_test(NAME krieg_daten
        COMMAND ${CMAKE_COMMAND} -DQUELLE=${CMAKE_CURRENT_SOURCE_DIR} -DZIEL=${krieg_daten}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/krieg_daten.cmake)
    set_tests_properties(krieg_daten PROPERTIES FIXTURES_SETUP krieg_daten)
    add_test(NAME pruefen_krieg_fixture COMMAND uhrwerk-cli pruefen --daten ${krieg_daten})
    add_test(NAME krieg_fixture COMMAND uhrwerk-cli krieg --daten ${krieg_daten} --seeds 16)
    set_tests_properties(pruefen_krieg_fixture krieg_fixture PROPERTIES FIXTURES_REQUIRED krieg_daten)
    set_tests_properties(krieg_fixture PROPERTIES PASS_REGULAR_EXPRESSION
        "Verstöße: keine – alle Kriegsziele erfüllt")
    add_test(NAME krieg_1848
        COMMAND uhrwerk-cli krieg --daten ${PROJECT_SOURCE_DIR}/data --szenario voelkerfruehling --seeds 1)
    # Ereignislabor 1848 (seit M11b): ein kurzer Lauf bis Ende 1848 prüft nur, dass das Labor mit dem Inhalt
    # rechnet (die Korridore gelten bis 1851 und prüft der CI-Job "Balancing" mit 16 Seeds).
    add_test(NAME ereignisse_1848
        COMMAND uhrwerk-cli ereignisse --daten ${PROJECT_SOURCE_DIR}/data --szenario voelkerfruehling --seeds 1
                --bis 1848.12.31)
    set_tests_properties(ereignisse_1848 PROPERTIES PASS_REGULAR_EXPRESSION
        "Märzunruhen \\(1848\\.03\\.01\\) → Vorparlament")

    # Ereignislabor (seit M11, docs/technik/ereignisse.md Abschnitt 7): eine kleine Revolution auf der Fixture
    # (tests/daten_ereignisse über tests/daten) gegen ihre Korridore über 16 Seeds, ohne Spieler und mit den
    # Strategien krone und reaktion für Brandenburg.
    set(ereignis_daten ${CMAKE_CURRENT_BINARY_DIR}/ereignis_daten)
    add_test(NAME ereignis_daten
        COMMAND ${CMAKE_COMMAND} -DQUELLE=${CMAKE_CURRENT_SOURCE_DIR} -DZIEL=${ereignis_daten}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/ereignis_daten.cmake)
    set_tests_properties(ereignis_daten PROPERTIES FIXTURES_SETUP ereignis_daten)
    add_test(NAME pruefen_ereignis_fixture COMMAND uhrwerk-cli pruefen --daten ${ereignis_daten})
    add_test(NAME ereignisse_fixture COMMAND uhrwerk-cli ereignisse --daten ${ereignis_daten} --seeds 16)
    add_test(NAME ereignisse_fixture_krone
        COMMAND uhrwerk-cli ereignisse --daten ${ereignis_daten} --seeds 16 --strategie krone)
    add_test(NAME ereignisse_fixture_reaktion
        COMMAND uhrwerk-cli ereignisse --daten ${ereignis_daten} --seeds 16 --strategie reaktion)
    set_tests_properties(pruefen_ereignis_fixture ereignisse_fixture ereignisse_fixture_krone
        ereignisse_fixture_reaktion PROPERTIES FIXTURES_REQUIRED ereignis_daten)
    set_tests_properties(ereignisse_fixture ereignisse_fixture_krone ereignisse_fixture_reaktion
        PROPERTIES PASS_REGULAR_EXPRESSION "Verstöße: keine – alle Korridore erfüllt")

    # Kriegslesezeichen (seit M10b, militaer.md Abschnitt 6.1): Daten, ein kurzer Lauf des Krieges aus
    # dem Lesezeichen und der Speichertest mitten im Krieg 1866. Der kurze Lauf prüft nur, dass das Labor
    # rechnet; die Korridore (seit b2) prüft das Kriegslabor mit 16 Seeds in der CI.
    foreach(lesezeichen dueppel koeniggraetz sedan)
        add_test(NAME daten_pruefen_${lesezeichen}
            COMMAND uhrwerk-cli pruefen --daten ${PROJECT_SOURCE_DIR}/data --szenario ${lesezeichen})
        add_test(NAME krieg_${lesezeichen}
            COMMAND uhrwerk-cli krieg --daten ${PROJECT_SOURCE_DIR}/data --szenario ${lesezeichen} --seeds 1
                    --tage 60)
        set_tests_properties(krieg_${lesezeichen} PROPERTIES PASS_REGULAR_EXPRESSION "Ausgang +Läufe +Anteil")
    endforeach()
    add_test(NAME speichertest_koeniggraetz
        COMMAND uhrwerk-cli speichertest --daten ${PROJECT_SOURCE_DIR}/data --szenario koeniggraetz --tage 90
                --punkte 3)

    # Spielstände und Wiederholungen (seit M6, docs/technik/speicherstaende.md Abschnitt 10): Abnahme
    # mit den echten Daten über die Datei; Wiederholung eines Protokolls und eines Spielstands über
    # 50 Jahre mit Testbefehlen.
    add_test(NAME speichertest_50_jahre
        COMMAND uhrwerk-cli speichertest --daten ${PROJECT_SOURCE_DIR}/data --jahre 50 --testbefehle 200
                --punkte 10 --threads 3)
    set(partie ${CMAKE_CURRENT_BINARY_DIR}/partie_50_jahre)
    add_test(NAME protokoll_50_jahre
        COMMAND uhrwerk-cli lauf --daten ${PROJECT_SOURCE_DIR}/data --jahre 50 --testbefehle 200
                --protokoll ${partie}.protokoll --speichern ${partie}.einigkeit)
    add_test(NAME wiederholung_50_jahre
        COMMAND uhrwerk-cli wiederholung ${partie}.protokoll --daten ${PROJECT_SOURCE_DIR}/data --threads 1)
    add_test(NAME wiederholung_spielstand
        COMMAND uhrwerk-cli wiederholung ${partie}.einigkeit --daten ${PROJECT_SOURCE_DIR}/data)
    add_test(NAME spielstand_laden
        COMMAND uhrwerk-cli spielstand ${partie}.einigkeit --daten ${PROJECT_SOURCE_DIR}/data)
    # Lesezeichen 1848 (seit M7): Verwaltung und Bundesgebiet stehen im Fingerabdruck; ein Jahr genügt.
    add_test(NAME speichertest_1848
        COMMAND uhrwerk-cli speichertest --daten ${PROJECT_SOURCE_DIR}/data --szenario voelkerfruehling
                --jahre 1 --testbefehle 20 --punkte 1)
    set_tests_properties(protokoll_50_jahre PROPERTIES FIXTURES_SETUP partie_50_jahre)
    set_tests_properties(wiederholung_50_jahre wiederholung_spielstand spielstand_laden
        PROPERTIES FIXTURES_REQUIRED partie_50_jahre)
    # Die Fixture-Daten sind ein anderer Inhalt: Der Spielstand muss abgelehnt werden.
    add_test(NAME spielstand_fremder_inhalt
        COMMAND uhrwerk-cli spielstand ${partie}.einigkeit --daten ${CMAKE_CURRENT_SOURCE_DIR}/daten)
    set_tests_properties(spielstand_fremder_inhalt PROPERTIES
        FIXTURES_REQUIRED partie_50_jahre
        PASS_REGULAR_EXPRESSION "passt nicht zu den Spieldaten \\(abweichend: ")
endif()
