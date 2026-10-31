cmake_minimum_required(VERSION 3.16)

project(attackshark-x11 VERSION 2.1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
    main.cpp
    core/types.h
    device/attackshark_device.h
    device/attackshark_device.cpp
    atsx11.h
    atsx11.cpp
    atsx11.ui
    settings.h
    settings.cpp
    settings.ui
    mouse_model_display.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(attackshark-x11
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(attackshark-x11
        ${PROJECT_SOURCES}
    )
endif()

target_include_directories(attackshark-x11 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(attackshark-x11 PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(attackshark-x11 PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

include(GNUInstallDirs)
install(TARGETS attackshark-x11
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES packaging/attackshark-x11.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES packaging/icons/attackshark-x11.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(attackshark-x11)
endif()
