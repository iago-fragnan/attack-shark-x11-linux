cmake_minimum_required(VERSION 3.16)

project(attackshark-x11 VERSION 2.1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets DBus)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets DBus)

set(PROJECT_SOURCES
    main.cpp
    core/types.h
    core/profile_store.h
    core/profile_store.cpp
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
target_link_libraries(attackshark-x11 PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus)

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

install(FILES packaging/70-attackshark.rules
    DESTINATION lib/udev/rules.d
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(attackshark-x11)
endif()

# ---------------------------------------------------------------------------
# Packaging (cpack -G DEB)
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "attackshark-x11")
set(CPACK_PACKAGE_VENDOR "Attack Shark X11 Linux contributors")
set(CPACK_PACKAGE_CONTACT "iago-fragnan")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Unofficial configuration utility for the Attack Shark X11 mouse")
set(CPACK_PACKAGE_HOMEPAGE_URL
    "https://github.com/iago-fragnan/attack-shark-x11-linux")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Attack Shark X11 Linux contributors")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")

# CPack normally asks dpkg for the target architecture, which is unavailable on
# non-Debian build hosts. Fall back to mapping the build machine's architecture.
if(NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(armv7l|armhf)$")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "armhf")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^riscv64$")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "riscv64")
    else()
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
endif()

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Minimum versions: the binary is built against Qt 6.4 (Debian 12 / Ubuntu
# 24.04) and requires glibc 2.34.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libc6 (>= 2.34), libqt6widgets6 (>= 6.4.2), libqt6dbus6 (>= 6.4.2)")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(CPACK_GENERATOR "DEB")
include(CPack)
