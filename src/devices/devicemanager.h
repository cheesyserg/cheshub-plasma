// SPDX-License-Identifier: GPL-3.0-or-later
// Finds supported devices among /dev/hidraw* and follows hotplug by watching
// /dev. Matching follows the web drivers' WebHID filters (vendor/product id
// plus the vendor collection), which on Linux is one hidraw node per device.
#pragma once

#include "abstractdevice.h"

#include <QFileSystemWatcher>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>

namespace cheshub {

class DeviceManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QList<QObject *> devices READ devices NOTIFY devicesChanged)
    // Devices that are present but cannot be configured, with the reason.
    Q_PROPERTY(QStringList notices READ notices NOTIFY devicesChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)

public:
    explicit DeviceManager(QObject *parent = nullptr);

    QList<QObject *> devices() const;
    QStringList notices() const { return m_notices; }
    bool dirty() const;

    Q_INVOKABLE void rescan();
    void syncAll();
    void applyAll();
    void loadDefaultsAll();

Q_SIGNALS:
    void devicesChanged();
    void dirtyChanged();

private:
    QList<AbstractDevice *> m_devices;
    QStringList m_notices;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};

} // namespace cheshub
