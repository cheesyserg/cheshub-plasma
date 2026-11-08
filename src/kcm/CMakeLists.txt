# SPDX-License-Identifier: GPL-3.0-or-later
# Embeds ui/*.qml and installs the plugin to plasma/kcms/systemsettings,
# where System Settings finds it under Input & Output → Input Devices.
kcmutils_add_qml_kcm(kcm_cheshub SOURCES cheshubkcm.cpp)
target_link_libraries(kcm_cheshub PRIVATE
    cheshub_devices
    KF6::KCMUtilsQuick
    KF6::CoreAddons
    KF6::I18n
    Qt6::Qml
    Qt6::Quick
)
target_compile_definitions(kcm_cheshub PRIVATE TRANSLATION_DOMAIN="kcm_cheshub")
