// SPDX-License-Identifier: GPL-3.0-or-later
#include "abstractdevice.h"

namespace cheshub {

AbstractDevice::AbstractDevice(const hid::NodeInfo &node, QObject *parent)
    : QObject(parent)
    , m_node(node)
    , m_worker(new IoWorker(this))
{
}

AbstractDevice::~AbstractDevice()
{
    // Subclasses stop the worker in their own destructor, before their
    // session goes away; this is a safety net.
    m_worker->stop();
}

void AbstractDevice::beginJob(const QString &status)
{
    ++m_jobs;
    m_status = status;
    Q_EMIT stateChanged();
}

void AbstractDevice::endJob(const QString &error)
{
    m_jobs = qMax(0, m_jobs - 1);
    m_error = error;
    if (m_jobs == 0)
        m_status.clear();
    Q_EMIT stateChanged();
}

void AbstractDevice::setReady(bool ready)
{
    if (m_ready == ready)
        return;
    m_ready = ready;
    Q_EMIT stateChanged();
}

void AbstractDevice::setTelemetry(int battery, bool charging)
{
    m_battery = battery;
    m_charging = charging;
    Q_EMIT stateChanged();
}

void AbstractDevice::setFirmware(const QString &firmware)
{
    m_firmware = firmware;
    Q_EMIT stateChanged();
}

void AbstractDevice::setStatus(const QString &status)
{
    m_status = status;
    Q_EMIT stateChanged();
}

} // namespace cheshub
