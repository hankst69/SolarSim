# Qt GUI application. Qt6 is preferred, Qt5 works as a fallback.
find_package(Qt6 COMPONENTS Widgets QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.12 COMPONENTS Widgets QUIET)
endif()

if(NOT Qt6_FOUND AND NOT Qt5_FOUND)
    message(STATUS "Qt (Widgets) not found - skipping the SolarSim GUI application")
    return()
endif()

set(CMAKE_AUTOMOC ON)

add_executable(SolarSim WIN32
    main.cpp
    MainWindow.cpp
    MainWindow.h
    SceneView.cpp
    SceneView.h
)

target_link_libraries(SolarSim PRIVATE geolib)

if(Qt6_FOUND)
    target_link_libraries(SolarSim PRIVATE Qt6::Widgets)
else()
    target_link_libraries(SolarSim PRIVATE Qt5::Widgets)
endif()

target_include_directories(SolarSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
