# Testdaten des Ereignislabors (seit M11, Schritt a5): die Fixture tests/daten mit den Ereignissen aus
# tests/daten_ereignisse darüber, frisch in den Build-Ordner kopiert (CTest ereignis_daten).
# Aufruf: cmake -DQUELLE=<tests> -DZIEL=<ordner> -P ereignis_daten.cmake
file(REMOVE_RECURSE "${ZIEL}")
file(COPY "${QUELLE}/daten/" DESTINATION "${ZIEL}")
file(COPY "${QUELLE}/daten_ereignisse/" DESTINATION "${ZIEL}")
