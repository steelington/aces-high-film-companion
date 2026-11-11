cmake_minimum_required(VERSION 3.21)
project(aces_high_film_companion LANGUAGES CXX)

# Aces High film companion: camera controls for the Aces High film viewer, through its socket only.
# A desktop window (QML) and a tablet page (web/control.html, served by the app) over one core.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network Qml Quick)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(film_companion WIN32
    src/main.cpp
    src/viewer_link.cpp src/viewer_link.h
    src/rig.cpp src/rig.h
    src/core.cpp src/core.h
    src/http_api.cpp src/http_api.h
    src/dock.cpp src/dock.h
)
qt_add_qml_module(film_companion
    URI Companion
    VERSION 1.0
    QML_FILES qml/Main.qml
    RESOURCES web/control.html
)
target_link_libraries(film_companion PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick)
if(WIN32)
    target_link_libraries(film_companion PRIVATE user32)
endif()
