# Testdaten des KI-Labors (seit M12, Schritt a5): die Fixture tests/daten mit den Militärdaten aus
# tests/daten_krieg, den Ereignissen aus tests/daten_ereignisse und der KI aus tests/daten_ki darüber, frisch
# in den Build-Ordner kopiert (CTest ki_daten).
# Aufruf: cmake -DQUELLE=<tests> -DZIEL=<ordner> -P ki_daten.cmake
file(REMOVE_RECURSE "${ZIEL}")
file(COPY "${QUELLE}/daten/" DESTINATION "${ZIEL}")
file(COPY "${QUELLE}/daten_krieg/" DESTINATION "${ZIEL}")
file(COPY "${QUELLE}/daten_ereignisse/" DESTINATION "${ZIEL}")
file(COPY "${QUELLE}/daten_ki/" DESTINATION "${ZIEL}")
