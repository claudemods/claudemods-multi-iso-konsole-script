cmake_minimum_required(VERSION 3.19)

project(cmiadvanced VERSION 3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.3 REQUIRED COMPONENTS Widgets Network)
qt_standard_project_setup()

# The three zip payloads (~42 MB) and the guide are embedded with rcc's
# "big resources" mode so they are compiled straight into an object file.
qt_add_big_resources(CMI_RESOURCES resources.qrc)

qt_add_executable(cmiadvanced
    src/main.cpp
    src/theme.h          src/theme.cpp
    src/config.h         src/config.cpp
    src/sudomanager.h    src/sudomanager.cpp
    src/console.h        src/console.cpp
    src/taskcontext.h    src/taskcontext.cpp
    src/dialogs.h        src/dialogs.cpp
    src/mainwindow.h     src/mainwindow.cpp
    ${CMI_RESOURCES}
)

target_link_libraries(cmiadvanced PRIVATE Qt6::Widgets Qt6::Network)

install(TARGETS cmiadvanced RUNTIME DESTINATION bin)
