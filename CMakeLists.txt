cmake_minimum_required(VERSION 3.16)

project(attackshark-x11 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(PROJECT_SOURCES
        main.cpp
        atsx11.cpp
        atsx11.h
        atsx11.ui
        dpiscale.h dpiscale.cpp
        dpibar.h dpibar.cpp
        autostart.h autostart.cpp
        systemtray.h systemtray.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(attackshark-x11
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        assets/mouse-2.png
        mouse_model_display.qrc
        hook.h hook.cpp
        settings.h settings.cpp settings.ui
        dpi.h dpi.cpp
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

target_link_libraries(attackshark-x11 PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

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

# Grants the logged-in user access to the mouse so it runs without root.
# Absolute on purpose: udev reads /etc/udev/rules.d whatever the install prefix
# is, while CMAKE_INSTALL_SYSCONFDIR is relative to it and would land the rule
# in, say, /usr/local/etc/udev/rules.d where nothing looks.
install(FILES packaging/98-attackshark-x11.rules
    DESTINATION /etc/udev/rules.d
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(attackshark-x11)
endif()


find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED libudev)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

target_include_directories(attackshark-x11 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${UDEV_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIRS})
target_link_libraries(attackshark-x11 PRIVATE ${UDEV_LIBRARIES} ${LIBUSB_LIBRARIES})

enable_testing()
add_executable(dpi_encoding_tests tests/dpi_encoding_test.cpp dpi.cpp)
target_compile_features(dpi_encoding_tests PRIVATE cxx_std_17)
add_test(NAME dpi_encoding_tests COMMAND dpi_encoding_tests)

add_executable(dpiscale_tests tests/dpiscale_test.cpp dpiscale.cpp)
target_compile_features(dpiscale_tests PRIVATE cxx_std_17)
add_test(NAME dpiscale_tests COMMAND dpiscale_tests)

add_executable(autostart_tests tests/autostart_test.cpp autostart.cpp)
target_compile_features(autostart_tests PRIVATE cxx_std_17)
target_link_libraries(autostart_tests PRIVATE Qt${QT_VERSION_MAJOR}::Core)
add_test(NAME autostart_tests COMMAND autostart_tests)

add_executable(battery_packet_tests tests/battery_packet_test.cpp hook.cpp dpi.cpp)
target_compile_features(battery_packet_tests PRIVATE cxx_std_17)
target_include_directories(battery_packet_tests PRIVATE ${UDEV_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIRS})
target_link_libraries(battery_packet_tests PRIVATE ${UDEV_LIBRARIES} ${LIBUSB_LIBRARIES} Qt${QT_VERSION_MAJOR}::Core)
add_test(NAME battery_packet_tests COMMAND battery_packet_tests)

add_executable(dpibar_interaction_tests tests/dpibar_interaction_test.cpp dpibar.cpp dpiscale.cpp)
target_compile_features(dpibar_interaction_tests PRIVATE cxx_std_17)
# QtTest ships with qtbase, so this pulls in no extra dependency.
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
target_link_libraries(dpibar_interaction_tests PRIVATE Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
add_test(NAME dpibar_interaction_tests COMMAND dpibar_interaction_tests)

add_executable(dpibar_label_tests tests/dpibar_labels_test.cpp dpibar.cpp dpiscale.cpp)
target_compile_features(dpibar_label_tests PRIVATE cxx_std_17)
target_link_libraries(dpibar_label_tests PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
add_test(NAME dpibar_label_tests COMMAND dpibar_label_tests)

add_executable(access_message_tests tests/access_message_test.cpp hook.cpp dpi.cpp)
target_compile_features(access_message_tests PRIVATE cxx_std_17)
target_include_directories(access_message_tests PRIVATE ${UDEV_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIRS})
target_link_libraries(access_message_tests PRIVATE ${UDEV_LIBRARIES} ${LIBUSB_LIBRARIES} Qt${QT_VERSION_MAJOR}::Core)
add_test(NAME access_message_tests COMMAND access_message_tests)
