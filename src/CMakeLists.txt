# SPDX-License-Identifier: GPL-3.0-or-later

# Hardware layer: byte-exact protocol encoders, hidraw transport and the
# blocking device sessions. Plain C++20, no Qt.
add_library(cheshub_core STATIC
    protocol/tkgm012.cpp
    protocol/b75.cpp
    protocol/b75keys.cpp
    hid/reportdescriptor.cpp
    hid/hidraw.cpp
    drivers/tkgm012session.cpp
    drivers/b75session.cpp
)
target_include_directories(cheshub_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# KDECompilerSettings turns exceptions off; the core does not use them.
set_target_properties(cheshub_core PROPERTIES AUTOMOC OFF)

# Qt models on top of the sessions, shared by the KCM.
add_library(cheshub_devices STATIC
    devices/ioworker.cpp
    devices/abstractdevice.cpp
    devices/tkgm012device.cpp
    devices/b75device.cpp
    devices/devicemanager.cpp
)
target_link_libraries(cheshub_devices PUBLIC cheshub_core Qt6::Core Qt6::Gui)

if(BUILD_CLI)
    add_executable(cheshubctl tools/cheshubctl.cpp)
    target_link_libraries(cheshubctl PRIVATE cheshub_core)
    set_target_properties(cheshubctl PROPERTIES AUTOMOC OFF)
    install(TARGETS cheshubctl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(BUILD_BATTERYD)
    add_library(cheshub_batteryd_lib STATIC batteryd/virtualbattery.cpp)
    target_link_libraries(cheshub_batteryd_lib PUBLIC cheshub_core)
    set_target_properties(cheshub_batteryd_lib PROPERTIES AUTOMOC OFF)

    add_executable(cheshub-batteryd batteryd/main.cpp)
    target_link_libraries(cheshub-batteryd PRIVATE cheshub_batteryd_lib)
    set_target_properties(cheshub-batteryd PROPERTIES AUTOMOC OFF)

    # /usr/libexec, not KDE's ${LIBDIR}/libexec: on Fedora a binary under
    # /usr/lib64 is labelled as a library by SELinux, and systemd then
    # cannot start it in the normal service domain.
    set(BATTERYD_INSTALL_DIR "libexec" CACHE PATH "Where to install cheshub-batteryd (relative to the prefix)")
    set(BATTERYD_DIR ${BATTERYD_INSTALL_DIR})
    install(TARGETS cheshub-batteryd RUNTIME DESTINATION ${BATTERYD_DIR})
    if(IS_ABSOLUTE "${BATTERYD_DIR}")
        set(BATTERYD_PATH "${BATTERYD_DIR}/cheshub-batteryd")
    else()
        set(BATTERYD_PATH "${CMAKE_INSTALL_PREFIX}/${BATTERYD_DIR}/cheshub-batteryd")
    endif()
    configure_file(${PROJECT_SOURCE_DIR}/data/cheshub-batteryd.service.in
                   ${CMAKE_CURRENT_BINARY_DIR}/cheshub-batteryd.service @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cheshub-batteryd.service DESTINATION ${SYSTEMD_UNIT_DIR})
endif()

if(BUILD_KCM)
    add_subdirectory(kcm)
endif()
