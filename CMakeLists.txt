cmake_minimum_required(VERSION 3.16)

project(attackshark-x11 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        atsx11.cpp
        atsx11.h
        atsx11.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(attackshark-x11
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        assets/mouse-2.png
        mouse_model_display.qrc
        hook.h hook.cpp
        settings.h settings.cpp settings.ui




    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET attackshark-x11 APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(attackshark-x11 SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(attackshark-x11
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(attackshark-x11 PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.attackshark-x11)
endif()
set_target_properties(attackshark-x11 PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

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


find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED libudev)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

target_include_directories(attackshark-x11 PRIVATE ${UDEV_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIRS})
target_link_libraries(attackshark-x11 PRIVATE ${UDEV_LIBRARIES} ${LIBUSB_LIBRARIES})