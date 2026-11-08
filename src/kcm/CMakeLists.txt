# SPDX-License-Identifier: GPL-3.0-or-later
# Two modules from one source, each placed where KDE keeps that kind of
# device: kcm_cheshub_mouse under Mouse & Touchpad (pointing-devices) and
# kcm_cheshub_keyboard under Keyboard. kcmutils_add_qml_kcm embeds ui/*.qml
# and installs to plasma/kcms/systemsettings.
foreach(kind mouse keyboard)
    set(target kcm_cheshub_${kind})
    kcmutils_add_qml_kcm(${target} SOURCES cheshubkcm.cpp)
    target_link_libraries(${target} PRIVATE
        cheshub_devices
        KF6::KCMUtilsQuick
        KF6::CoreAddons
        KF6::I18n
        Qt6::Qml
        Qt6::Quick
    )
    target_compile_definitions(${target} PRIVATE
        TRANSLATION_DOMAIN="kcm_cheshub"
        CHESHUB_KIND="${kind}"
        CHESHUB_JSON="${target}.json"
    )
endforeach()
