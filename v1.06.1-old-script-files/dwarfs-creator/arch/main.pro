# Project name
TARGET = dwarfs.bin

# Project type
TEMPLATE = app

# Source files
SOURCES += main.cpp

# Include paths
INCLUDEPATH += .

# Libraries
LIBS += -lstdc++fs  # For std::filesystem support

# Compiler flags
QMAKE_CXXFLAGS += -std=c++17

# Additional settings
CONFIG += c++17

# Uncomment the following lines if you need to link against additional libraries
# LIBS += -lrsync -lmksquashfs -lmkinitcpio
