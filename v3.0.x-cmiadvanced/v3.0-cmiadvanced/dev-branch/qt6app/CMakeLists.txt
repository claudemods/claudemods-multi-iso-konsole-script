cmake_minimum_required(VERSION 3.19)

project(cmiadvanced VERSION 3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.3 REQUIRED COMPONENTS Widgets Network)
qt_standard_project_setup()

# Folder holding build-image-arch-img.zip, calamares-files.zip and claudemods.zip
# (the parent of this qt6 folder by default). Override with -DCMI_DATA_DIR=/path
set(CMI_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "Folder containing the cmiadvanced zip payloads")
get_filename_component(CMI_DATA_DIR "${CMI_DATA_DIR}" ABSOLUTE)

set(CMI_ZIPS build-image-arch-img.zip calamares-files.zip claudemods.zip)
set(CMI_QRC_ENTRIES "")
foreach(zip IN LISTS CMI_ZIPS)
    set(zip_path "${CMI_DATA_DIR}/${zip}")
    if(NOT EXISTS "${zip_path}")
        message(FATAL_ERROR "Missing ${zip_path}\n"
                            "Put the zip files next to this folder or pass -DCMI_DATA_DIR=/folder/with/zips")
    endif()
    string(APPEND CMI_QRC_ENTRIES "        <file alias=\"zips/${zip}\">${zip_path}</file>\n")
endforeach()

# The guide is optional; the app shows ~/.config/cmi/readme.txt when it exists.
set(CMI_GUIDE_FILE "${CMI_DATA_DIR}/guide/readme.txt" CACHE FILEPATH "Guide shown when ~/.config/cmi/readme.txt is missing")
if(EXISTS "${CMI_GUIDE_FILE}")
    get_filename_component(CMI_GUIDE_FILE "${CMI_GUIDE_FILE}" ABSOLUTE)
    string(APPEND CMI_QRC_ENTRIES "        <file alias=\"guide/readme.txt\">${CMI_GUIDE_FILE}</file>\n")
else()
    message(STATUS "No guide found at ${CMI_GUIDE_FILE} - building without a built-in guide")
endif()

# The zip payloads (~42 MB) are embedded with rcc's "big resources" mode so
# they are compiled straight into an object file.
set(CMI_QRC "${CMAKE_CURRENT_BINARY_DIR}/resources.qrc")
file(WRITE "${CMI_QRC}" "<!DOCTYPE RCC>\n<RCC version=\"1.0\">\n    <qresource prefix=\"/\">\n${CMI_QRC_ENTRIES}    </qresource>\n</RCC>\n")
qt_add_big_resources(CMI_RESOURCES "${CMI_QRC}")

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

# util: forkpty() (commands run in a real terminal so progress bars animate)
target_link_libraries(cmiadvanced PRIVATE Qt6::Widgets Qt6::Network util)

install(TARGETS cmiadvanced RUNTIME DESTINATION bin)
