# Unit tests for the SolarSim Qt GUI application (src/app).
#
# These link against SolarSimAppLib, the static library that src/app's
# CMakeLists.txt builds from everything except main.cpp, so the tests exercise
# the exact same MainWindow/SceneView/HeightDataSources code as the shipped
# application without needing a second QApplication entry point.
#
# The tests create real Qt widgets, so they need a QApplication; they run with
# the "offscreen" QPA platform plugin so they work headlessly (CI, no display).

function(add_app_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE SolarSimAppLib)
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/src/app)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

add_app_test(HeightDataSourcesTests HeightDataSourcesTests.cpp)
add_app_test(SceneViewTests SceneViewTests.cpp)
add_app_test(MainWindowTests MainWindowTests.cpp)
