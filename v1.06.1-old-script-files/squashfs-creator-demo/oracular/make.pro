TARGET = SquashFS_Creator
SOURCES += main.cpp
INCLUDEPATH += .
LIBS += -lstdc++fs
QMAKE_CXXFLAGS += -std=c++17
DESTDIR = $$PWD/bin
