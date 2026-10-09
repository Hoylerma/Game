add_executable(uhrwerk-cli main.cpp)
target_link_libraries(uhrwerk-cli PRIVATE uhrwerk::core uhrwerk_warnings)
