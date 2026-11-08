# SPDX-License-Identifier: GPL-3.0-or-later
# System Settings → Input & Output → ChesHub Devices: a list of the connected
# supported devices, each opening its own page. kcmutils_add_qml_kcm embeds
# ui/*.qml and installs to plasma/kcms/systemsettings.
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
