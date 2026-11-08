// SPDX-License-Identifier: GPL-3.0-or-later
// TECKNET TK-GM012 for QML. Feature set follows the vendor descriptor
// (device/mouse_tk-gm012.xml): no lighting page, no LOD / sensor / debounce
// (show_lod="0" show_sensor="0" show_respondtime="0"). The only LED is the
// DPI indicator, whose colour is set per stage.
#pragma once

#include "../drivers/tkgm012session.h"
#include "abstractdevice.h"

#include <QColor>
#include <QVariantList>

#include <memory>

namespace cheshub {

class TkGm012Device : public AbstractDevice
{
    Q_OBJECT
    Q_PROPERTY(int rate READ rate WRITE setRate NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList rates READ rates CONSTANT)
    Q_PROPERTY(int stageCount READ stageCount WRITE setStageCount NOTIFY settingsChanged)
    Q_PROPERTY(int activeStage READ activeStage WRITE setActiveStage NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList dpi READ dpi NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList stageColors READ stageColors NOTIFY settingsChanged)
    Q_PROPERTY(int dpiMin READ dpiMin CONSTANT)
    Q_PROPERTY(int dpiMax READ dpiMax CONSTANT)
    Q_PROPERTY(int sleepMinutes READ sleepMinutes WRITE setSleepMinutes NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList sleepOptions READ sleepOptions CONSTANT)
    Q_PROPERTY(bool moveWake READ moveWake WRITE setMoveWake NOTIFY settingsChanged)
    // [{ label, action, actionLabel, custom }] in visual order
    Q_PROPERTY(QVariantList buttons READ buttons NOTIFY settingsChanged)
    // [{ id, label }]
    Q_PROPERTY(QVariantList actions READ actions CONSTANT)

public:
    static bool matches(const hid::NodeInfo &node) { return TkGm012Session::matches(node); }

    explicit TkGm012Device(const hid::NodeInfo &node, QObject *parent = nullptr);
    ~TkGm012Device() override;

    QString kind() const override { return QStringLiteral("mouse"); }
    QString name() const override { return QStringLiteral("TECKNET TK-GM012"); }
    QString link() const override;
    bool dirty() const override { return dirtyRegisters() != 0; }

    int rate() const { return m_settings.rateHz; }
    void setRate(int hz);
    QVariantList rates() const;
    int stageCount() const { return m_settings.stageCount; }
    void setStageCount(int count);
    int activeStage() const { return m_settings.activeStage; }
    void setActiveStage(int stage);
    QVariantList dpi() const;
    QVariantList stageColors() const;
    int dpiMin() const { return tkgm012::DpiMin; }
    int dpiMax() const { return tkgm012::DpiMax; }
    int sleepMinutes() const { return m_settings.sleepMinutes; }
    void setSleepMinutes(int minutes);
    QVariantList sleepOptions() const;
    bool moveWake() const { return m_settings.moveWake; }
    void setMoveWake(bool on);
    QVariantList buttons() const;
    QVariantList actions() const;

    Q_INVOKABLE void setDpi(int stage, int value);
    Q_INVOKABLE void setStageColor(int stage, const QColor &color);
    // visualIndex follows `buttons`; actionId from `actions`.
    Q_INVOKABLE void setButtonAction(int visualIndex, const QString &actionId);
    Q_INVOKABLE int snapDpi(int value) const { return tkgm012::snapDpi(value); }

    void sync() override;
    void apply() override;
    void loadDefaults() override;
    void factoryReset() override;

private:
    unsigned dirtyRegisters() const;
    void edit(const std::function<void(tkgm012::Settings &)> &change);

    std::shared_ptr<TkGm012Session> m_session;
    bool m_opened = false;
    tkgm012::Settings m_settings; // what the UI shows
    tkgm012::Settings m_device; // what the mouse holds, as far as we know
};

} // namespace cheshub
