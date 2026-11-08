// SPDX-License-Identifier: GPL-3.0-or-later
// One background thread per device. HID transactions block for up to a few
// seconds (the 2.4 GHz links are slow to acknowledge), so they never run on
// the UI thread. Jobs run in order; the result is delivered back to `context`
// on its own thread.
#pragma once

#include <QObject>
#include <QPointer>
#include <QThread>

#include <functional>

namespace cheshub {

class IoWorker : public QObject
{
    Q_OBJECT
public:
    explicit IoWorker(QObject *parent = nullptr);
    ~IoWorker() override;

    // Waits for the running job and drops queued ones. Call before the objects
    // the jobs reference are destroyed.
    void stop();

    template<typename Job, typename Done>
    void run(QObject *context, Job job, Done done)
    {
        QPointer<QObject> ctx(context);
        post([ctx, job = std::move(job), done = std::move(done)]() mutable {
            auto result = job();
            if (ctx)
                QMetaObject::invokeMethod(ctx.data(), [done = std::move(done), result = std::move(result)]() mutable { done(result); }, Qt::QueuedConnection);
        });
    }

private:
    void post(std::function<void()> fn);

    QThread m_thread;
    QObject *m_target = nullptr; // lives on m_thread
    bool m_stopped = false;
};

} // namespace cheshub
