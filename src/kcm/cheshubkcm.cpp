// SPDX-License-Identifier: GPL-3.0-or-later
#include "cheshubkcm.h"

#include "../devices/b75device.h"
#include "../devices/tkgm012device.h"

#include <KPluginFactory>

#include <QQmlEngine>

K_PLUGIN_CLASS_WITH_JSON(cheshub::CheshubKcm, "kcm_cheshub.json")

namespace cheshub {

CheshubKcm::CheshubKcm(QObject *parent, const KPluginMetaData &data)
    : KQuickConfigModule(parent, data)
    , m_manager(new DeviceManager(this))
{
    setButtons(Apply | Default);

    const char *uri = "org.cheshub.kcm";
    qmlRegisterUncreatableType<AbstractDevice>(uri, 1, 0, "AbstractDevice", QStringLiteral("Provided by the module"));
    qmlRegisterUncreatableType<TkGm012Device>(uri, 1, 0, "TkGm012Device", QStringLiteral("Provided by the module"));
    qmlRegisterUncreatableType<B75Device>(uri, 1, 0, "B75Device", QStringLiteral("Provided by the module"));
    qmlRegisterUncreatableType<DeviceManager>(uri, 1, 0, "DeviceManager", QStringLiteral("Provided by the module"));

    connect(m_manager, &DeviceManager::dirtyChanged, this, [this]() { setNeedsSave(m_manager->dirty()); });
}

void CheshubKcm::load()
{
    // First load enumerates (and reads every device found); later loads are
    // the Reset button and re-read the hardware.
    if (!m_scanned) {
        m_scanned = true;
        m_manager->rescan();
    } else {
        m_manager->syncAll();
    }
    setNeedsSave(false);
}

void CheshubKcm::save()
{
    m_manager->applyAll();
}

void CheshubKcm::defaults()
{
    m_manager->loadDefaultsAll();
}

} // namespace cheshub

#include "cheshubkcm.moc"
