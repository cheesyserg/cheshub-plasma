// SPDX-License-Identifier: GPL-3.0-or-later
// Common surface every device exposes to QML. Device pages bind to these plus
// the model-specific properties of the subclass.
#pragma once

#include "../hid/hidraw.h"
#include "ioworker.h"

#include <QObject>
#include <QString>

namespace cheshub {

class AbstractDevice : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString kind READ kind CONSTANT) // "mouse" | "keyboard"
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString link READ link CONSTANT)
    Q_PROPERTY(QString devnode READ devnode CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged) // opened and read once
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(int battery READ battery NOTIFY stateChanged) // -1 = unknown
    Q_PROPERTY(bool charging READ charging NOTIFY stateChanged)
    Q_PROPERTY(QString firmware READ firmware NOTIFY stateChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)

public:
    explicit AbstractDevice(const hid::NodeInfo &node, QObject *parent = nullptr);
    ~AbstractDevice() override;

    virtual QString kind() const = 0;
    virtual QString name() const = 0;
    virtual QString link() const = 0;
    QString devnode() const { return QString::fromStdString(m_node.devnode); }

    bool busy() const { return m_jobs > 0; }
    bool ready() const { return m_ready; }
    QString error() const { return m_error; }
    QString status() const { return m_status; }
    int battery() const { return m_battery; }
    bool charging() const { return m_charging; }
    QString firmware() const { return m_firmware; }
    virtual bool dirty() const = 0;

    // Reads everything from the hardware.
    Q_INVOKABLE virtual void sync() = 0;
    // Writes whatever changed.
    Q_INVOKABLE virtual void apply() = 0;
    // UI state back to factory values (not written until apply()).
    Q_INVOKABLE virtual void loadDefaults() = 0;
    // Hardware factory reset, then re-read.
    Q_INVOKABLE virtual void factoryReset() = 0;

Q_SIGNALS:
    void stateChanged();
    void dirtyChanged();
    void settingsChanged();
    void applied(bool ok, const QString &message);

protected:
    void beginJob(const QString &status);
    void endJob(const QString &error = {});
    void setReady(bool ready);
    void setTelemetry(int battery, bool charging);
    void setFirmware(const QString &firmware);
    void setStatus(const QString &status);

    hid::NodeInfo m_node;
    IoWorker *m_worker;

private:
    int m_jobs = 0;
    bool m_ready = false;
    QString m_error;
    QString m_status;
    int m_battery = -1;
    bool m_charging = false;
    QString m_firmware;
};

} // namespace cheshub
