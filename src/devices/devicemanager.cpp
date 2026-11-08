// SPDX-License-Identifier: GPL-3.0-or-later
#include "devicemanager.h"

#include "../drivers/b75session.h"
#include "b75device.h"
#include "tkgm012device.h"

#include <QSet>

namespace cheshub {

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
{
    // hidraw nodes appear a moment before udev applies the access rule, so
    // wait a little after /dev changes.
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(800);
    connect(&m_debounce, &QTimer::timeout, this, &DeviceManager::rescan);
    m_watcher.addPath(QStringLiteral("/dev"));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this]() { m_debounce.start(); });
}

QList<QObject *> DeviceManager::devices() const
{
    QList<QObject *> out;
    for (AbstractDevice *d : m_devices)
        out << d;
    return out;
}

bool DeviceManager::dirty() const
{
    for (AbstractDevice *d : m_devices) {
        if (d->dirty())
            return true;
    }
    return false;
}

void DeviceManager::rescan()
{
    const auto nodes = hid::enumerate();
    QSet<QString> present;
    QStringList notices;
    QList<AbstractDevice *> added;

    auto known = [this](const QString &devnode) {
        for (AbstractDevice *d : m_devices) {
            if (d->devnode() == devnode)
                return true;
        }
        return false;
    };

    for (const hid::NodeInfo &node : nodes) {
        const QString devnode = QString::fromStdString(node.devnode);
        AbstractDevice *device = nullptr;
        if (TkGm012Device::matches(node)) {
            present.insert(devnode);
            if (!known(devnode))
                device = new TkGm012Device(node, this);
        } else if (B75Device::matches(node)) {
            present.insert(devnode);
            if (!known(devnode))
                device = new B75Device(node, this);
        } else if (B75Session::linkFor(node) == B75Session::Link::Bluetooth) {
            notices << QStringLiteral("iLovbee B75 is connected over Bluetooth, which is read-only on this keyboard. "
                                      "Connect the USB cable or the 2.4 GHz receiver to configure it.");
        }
        if (device) {
            connect(device, &AbstractDevice::dirtyChanged, this, &DeviceManager::dirtyChanged);
            added << device;
        }
    }

    bool changed = !added.isEmpty() || notices != m_notices;
    for (auto it = m_devices.begin(); it != m_devices.end();) {
        if (!present.contains((*it)->devnode())) {
            (*it)->deleteLater();
            it = m_devices.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    m_devices << added;
    m_notices = notices;
    if (changed) {
        Q_EMIT devicesChanged();
        Q_EMIT dirtyChanged();
    }
    for (AbstractDevice *d : added)
        d->sync();
}

void DeviceManager::syncAll()
{
    for (AbstractDevice *d : m_devices)
        d->sync();
}

void DeviceManager::applyAll()
{
    for (AbstractDevice *d : m_devices)
        d->apply();
}

void DeviceManager::loadDefaultsAll()
{
    for (AbstractDevice *d : m_devices)
        d->loadDefaults();
}

} // namespace cheshub
