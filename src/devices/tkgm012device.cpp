// SPDX-License-Identifier: GPL-3.0-or-later
#include "tkgm012device.h"

#include <QVariantMap>

namespace cheshub {

using namespace tkgm012;

TkGm012Device::TkGm012Device(const hid::NodeInfo &node, QObject *parent)
    : AbstractDevice(node, parent)
    , m_session(std::make_shared<TkGm012Session>())
{
}

TkGm012Device::~TkGm012Device()
{
    m_worker->stop();
}

QString TkGm012Device::link() const
{
    return TkGm012Session::isWireless(m_node) ? QStringLiteral("2.4 GHz receiver") : QStringLiteral("USB cable");
}

unsigned TkGm012Device::dirtyRegisters() const
{
    if (!ready())
        return 0;
    unsigned regs = 0;
    const Settings &a = m_settings;
    const Settings &b = m_device;
    if (a.rateHz != b.rateHz)
        regs |= TkGm012Session::RegRate;
    if (a.stageCount != b.stageCount || a.activeStage != b.activeStage || a.dpi != b.dpi)
        regs |= TkGm012Session::RegDpi;
    if (a.colors != b.colors)
        regs |= TkGm012Session::RegColors;
    if (a.sleepMinutes != b.sleepMinutes || a.moveWake != b.moveWake)
        regs |= TkGm012Session::RegPower;
    if (a.keys != b.keys)
        regs |= TkGm012Session::RegKeys;
    return regs;
}

void TkGm012Device::edit(const std::function<void(Settings &)> &change)
{
    const bool wasDirty = dirty();
    Settings next = m_settings;
    change(next);
    next.stageCount = qBound(1, next.stageCount, StageCount);
    next.activeStage = qBound(0, next.activeStage, next.stageCount - 1);
    if (next == m_settings)
        return;
    m_settings = next;
    Q_EMIT settingsChanged();
    if (wasDirty != dirty())
        Q_EMIT dirtyChanged();
}

// --------------------------------------------------------------- properties

void TkGm012Device::setRate(int hz)
{
    edit([hz](Settings &s) { s.rateHz = decodeRate(encodeRate(hz)).value_or(1000); });
}

QVariantList TkGm012Device::rates() const
{
    QVariantList out;
    for (int hz : SupportedRates)
        out << hz;
    return out;
}

void TkGm012Device::setStageCount(int count)
{
    edit([count](Settings &s) { s.stageCount = count; });
}

void TkGm012Device::setActiveStage(int stage)
{
    edit([stage](Settings &s) { s.activeStage = stage; });
}

QVariantList TkGm012Device::dpi() const
{
    QVariantList out;
    for (int v : m_settings.dpi)
        out << v;
    return out;
}

QVariantList TkGm012Device::stageColors() const
{
    QVariantList out;
    for (Rgb c : m_settings.colors)
        out << QColor(red(c), green(c), blue(c));
    return out;
}

void TkGm012Device::setSleepMinutes(int minutes)
{
    edit([minutes](Settings &s) { s.sleepMinutes = nearestSleep(minutes); });
}

QVariantList TkGm012Device::sleepOptions() const
{
    QVariantList out;
    for (int m : SleepOptions)
        out << m;
    return out;
}

void TkGm012Device::setMoveWake(bool on)
{
    edit([on](Settings &s) { s.moveWake = on; });
}

QVariantList TkGm012Device::buttons() const
{
    QVariantList out;
    for (const Button &b : tkgm012::buttons()) {
        const KeyRecord &rec = m_settings.keys[b.keyValue];
        const Action *a = actionForRecord(rec);
        QVariantMap m;
        m[QStringLiteral("label")] = QString::fromUtf8(b.label);
        m[QStringLiteral("action")] = a ? QString::fromUtf8(a->id) : QStringLiteral("custom");
        m[QStringLiteral("actionLabel")] = a ? QString::fromUtf8(a->label)
                                             : QString::fromStdString(recordClassName(rec)) + QStringLiteral(" (set by another tool)");
        m[QStringLiteral("custom")] = a == nullptr;
        out << m;
    }
    return out;
}

QVariantList TkGm012Device::actions() const
{
    QVariantList out;
    for (const Action &a : tkgm012::actions()) {
        QVariantMap m;
        m[QStringLiteral("id")] = QString::fromUtf8(a.id);
        m[QStringLiteral("label")] = QString::fromUtf8(a.label);
        out << m;
    }
    return out;
}

void TkGm012Device::setDpi(int stage, int value)
{
    if (stage < 0 || stage >= StageCount)
        return;
    edit([stage, value](Settings &s) { s.dpi[stage] = tkgm012::snapDpi(value); });
}

void TkGm012Device::setStageColor(int stage, const QColor &color)
{
    if (stage < 0 || stage >= StageCount || !color.isValid())
        return;
    edit([stage, color](Settings &s) { s.colors[stage] = makeRgb(color.red(), color.green(), color.blue()); });
}

void TkGm012Device::setButtonAction(int visualIndex, const QString &actionId)
{
    if (visualIndex < 0 || visualIndex >= ButtonCount)
        return;
    const Action *a = actionById(actionId.toStdString());
    if (!a)
        return; // "custom" keeps the record read from the mouse
    const int keyValue = tkgm012::buttons()[visualIndex].keyValue;
    edit([keyValue, a](Settings &s) { s.keys[keyValue] = a->record; });
}

// ---------------------------------------------------------------- hardware

namespace {
struct SyncResult {
    bool opened = false;
    bool ok = false;
    std::string error;
    TkGm012Session::State state;
};
} // namespace

void TkGm012Device::sync()
{
    beginJob(QStringLiteral("Reading from the mouse…"));
    auto session = m_session;
    const hid::NodeInfo node = m_node;
    const bool opened = m_opened;
    TkGm012Session::State start;
    start.settings = m_device;
    m_worker->run(
        this,
        [session, node, opened, start]() {
            SyncResult r;
            r.state = start;
            if (!opened && !session->open(node)) {
                r.error = session->error();
                return r;
            }
            r.opened = true;
            r.ok = session->readAll(r.state);
            if (!r.ok)
                r.error = session->error();
            return r;
        },
        [this](const SyncResult &r) {
            m_opened = r.opened;
            const bool wasDirty = dirty();
            if (r.state.info) {
                m_device = r.state.settings;
                m_settings = m_device;
                setTelemetry(r.state.info->battery.value_or(-1), r.state.info->charging);
                if (r.state.firmware)
                    setFirmware(QString::fromStdString(*r.state.firmware));
                setReady(true);
                Q_EMIT settingsChanged();
            }
            if (wasDirty != dirty())
                Q_EMIT dirtyChanged();
            endJob(QString::fromStdString(r.error));
        });
}

void TkGm012Device::apply()
{
    const unsigned regs = dirtyRegisters();
    if (!regs || !m_opened)
        return;
    beginJob(QStringLiteral("Writing to the mouse…"));
    auto session = m_session;
    const Settings target = m_settings;
    m_worker->run(
        this, [session, target, regs]() { return session->write(target, regs); },
        [this, target](const Result &r) {
            const bool wasDirty = dirty();
            if (r.ok)
                m_device = target;
            if (wasDirty != dirty())
                Q_EMIT dirtyChanged();
            const QString message = !r.ok ? QString::fromStdString(r.error)
                : r.silent                ? QStringLiteral("Sent, but the mouse did not acknowledge every packet")
                                          : QStringLiteral("Saved to the mouse");
            endJob(r.ok ? QString() : message);
            Q_EMIT applied(r.ok, message);
        });
}

void TkGm012Device::loadDefaults()
{
    edit([](Settings &s) { s = factoryDefaults(); });
}

void TkGm012Device::factoryReset()
{
    if (!m_opened)
        return;
    beginJob(QStringLiteral("Restoring factory settings…"));
    auto session = m_session;
    m_worker->run(
        this, [session]() { return session->factoryReset(); },
        [this](const Result &r) {
            endJob(r.ok ? QString() : QString::fromStdString(r.error));
            Q_EMIT applied(r.ok, r.ok ? QStringLiteral("Factory settings restored") : QString::fromStdString(r.error));
            sync();
        });
}

} // namespace cheshub
