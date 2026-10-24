# Testdaten des Kriegslabors (seit M10, Schritt a5): die Fixture tests/daten mit den Militärdaten aus
# tests/daten_krieg darüber, frisch in den Build-Ordner kopiert (CTest krieg_daten).
# Aufruf: cmake -DQUELLE=<tests> -DZIEL=<ordner> -P krieg_daten.cmake
file(REMOVE_RECURSE "${ZIEL}")
file(COPY "${QUELLE}/daten/" DESTINATION "${ZIEL}")
file(COPY "${QUELLE}/daten_krieg/" DESTINATION "${ZIEL}")
