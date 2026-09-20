cmake_minimum_required(VERSION 3.21)
project(ReelForge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Sirf Widgets + Gui + Core -- koi QtNetwork/QtWebEngine/QtWebView kahin
# import nahi hota. Poora app local disk (QFile/QDir/QProcess) hi use karta hai.
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)
qt_standard_project_setup()

file(GLOB_RECURSE REELFORGE_SOURCES CONFIGURE_DEPENDS
    src/*.cpp src/*.h)

qt_add_executable(ReelForge WIN32
    ${REELFORGE_SOURCES}
)

target_link_libraries(ReelForge PRIVATE Qt6::Widgets Qt6::Gui Qt6::Core)
target_include_directories(ReelForge PRIVATE src)

if (WIN32)
    set_target_properties(ReelForge PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
