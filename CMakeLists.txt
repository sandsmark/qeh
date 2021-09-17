cmake_minimum_required(VERSION 3.5)

project(qeh VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)

add_executable(qeh main.cpp Viewer.cpp Viewer.h)

target_link_libraries(qeh PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
