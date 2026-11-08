// SPDX-License-Identifier: GPL-3.0-or-later
#include "ioworker.h"

namespace cheshub {

IoWorker::IoWorker(QObject *parent)
    : QObject(parent)
{
    m_target = new QObject;
    m_target->moveToThread(&m_thread);
    m_thread.setObjectName(QStringLiteral("cheshub-io"));
    m_thread.start();
}

IoWorker::~IoWorker()
{
    stop();
}

void IoWorker::stop()
{
    if (m_stopped)
        return;
    m_stopped = true;
    m_thread.quit();
    m_thread.wait();
    delete m_target; // the thread has finished, so this is safe here
    m_target = nullptr;
}

void IoWorker::post(std::function<void()> fn)
{
    if (m_stopped)
        return;
    QMetaObject::invokeMethod(m_target, std::move(fn), Qt::QueuedConnection);
}

} // namespace cheshub
