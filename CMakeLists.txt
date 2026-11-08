# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.22)
project(cheshub-plasma VERSION 0.2.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_KCM "Build the Plasma 6 System Settings module (needs KF6)" ON)
option(BUILD_CLI "Build the cheshubctl command-line tool" ON)
option(BUILD_TESTING "Build the protocol unit tests" ON)
option(BUILD_BATTERYD "Build cheshub-batteryd (device batteries in UPower / Power & Battery)" ON)

if(BUILD_KCM)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
    include(KDEInstallDirs)
    include(KDECMakeSettings)
    include(KDECompilerSettings NO_POLICY_SCOPE)
    include(FeatureSummary)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick)
    find_package(KF6 6.0 REQUIRED COMPONENTS KCMUtils CoreAddons I18n)
else()
    include(GNUInstallDirs)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui)
endif()

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
endif()

set(CMAKE_AUTOMOC ON)

set(SYSTEMD_UNIT_DIR "lib/systemd/system" CACHE PATH "Where to install systemd units (relative to the prefix)")

add_subdirectory(src)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# udev rule: lets the logged-in user open the devices' /dev/hidraw* nodes,
# the same rule the ChesHub web app documents.
set(UDEV_RULES_DIR "lib/udev/rules.d" CACHE PATH "Where to install udev rules (relative to the prefix)")
install(FILES data/70-cheshub-plasma.rules DESTINATION ${UDEV_RULES_DIR})

if(BUILD_KCM)
    feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()

# ------------------------------------------------------------------ packages
set(CPACK_PACKAGE_NAME "cheshub-plasma")
set(CPACK_PACKAGE_VENDOR "cheesyserg")
set(CPACK_PACKAGE_CONTACT "https://github.com/cheesyserg/cheshub-plasma")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Plasma 6 settings for TECKNET TK-GM012 mice and iLovbee B75 keyboards")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "kde")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/deb/postinst"
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/deb/prerm")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/post-install.sh")
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/rpm-preun.sh")
# Directories owned by the base system on Fedora.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/lib/udev /usr/lib/udev/rules.d /usr/lib/systemd /usr/lib/systemd/system
    /usr/libexec /usr/lib64/libexec /usr/lib/libexec /usr/lib64/qt6 /usr/lib64/qt6/plugins
    /usr/lib64/qt6/plugins/plasma /usr/lib64/qt6/plugins/plasma/kcms
    /usr/lib64/qt6/plugins/plasma/kcms/systemsettings /usr/share/applications)
include(CPack)
