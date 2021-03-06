cmake_minimum_required(VERSION 2.8)
project(viewer)

#
#
# Qt
#
#
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_FLAGS "-std=c++11")

find_package(OpenGL REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5OpenGL REQUIRED)

#
#
# hpgv
#
#
link_directories(${CMAKE_BINARY_DIR}/hpgv)

#
#
# SOURCES
#
#
set(SHADERS
        shaders/raf.vert
        shaders/raf.frag
        shaders/normal.vert
        shaders/normal.frag
        shaders/aware.vert
        shaders/aware.frag
        shaders/iso.vert
        shaders/iso.frag
        shaders/isoraf.vert
        shaders/isoraf.frag)
set(SOURCES
        TF.cpp
        TFEditor.cpp
        main.cpp
        mainwindow.cpp
        mainwindow.ui
        viewer.cpp
        featuretracker.cpp
        lightctrl.cpp
        imagecache.cpp
        markslider.cpp
        keyframe.cpp)

#
#
# Qt Resources
#
#
qt5_add_resources(SHADERS res.qrc)

#
#
# Executable
#
#
add_executable(hpgview ${SOURCES} ${SHADERS})
qt5_use_modules(hpgview Core Widgets OpenGL)
qt5_wrap_ui(ui_mainwindow.h mainwindow.ui)
target_link_libraries(hpgview hpgv ${HDF5_LIBRARIES} ${OPENGL_LIBRARIES})

#
#
# Install
#
#
install(TARGETS hpgview DESTINATION bin)