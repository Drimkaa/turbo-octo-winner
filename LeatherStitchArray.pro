QT += core gui widgets
TEMPLATE = lib
CONFIG += plugin c++17
VERSION = 1.0.0
TARGET = $$qtLibraryTarget(leatherstitcharray)

isEmpty(LIBRECAD_SOURCE) {
    error("Set LIBRECAD_SOURCE to a LibreCAD 2.2.1 source checkout")
}

INCLUDEPATH += $$PWD \
               $$PWD/src \
               $${LIBRECAD_SOURCE}/librecad/src/plugins

SOURCES += src/geometry.cpp \
           src/leatherstitcharray.cpp \
           src/stitchdialog.cpp

HEADERS += src/geometry.h \
           src/leatherstitcharray.h \
           src/stitchdialog.h

DISTFILES += leatherstitcharray.json

DESTDIR = $$PWD/build/plugin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
RCC_DIR = $$PWD/build/rcc
UI_DIR = $$PWD/build/ui
