TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle qt
TARGET = leather_stitch_geometry_tests

INCLUDEPATH += ../src
SOURCES += test_geometry.cpp \
           ../src/geometry.cpp
HEADERS += ../src/geometry.h
