
TEMPLATE = app

QT += core

CONFIG += c++17

SOURCES += main.cpp
HEADERS += policy.h

INCLUDEPATH += ../../src
LIBS += -lev

QMAKE_CXXFLAGS += -Wall -Wextra
