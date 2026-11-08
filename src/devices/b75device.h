// SPDX-License-Identifier: GPL-3.0-or-later
// iLovbee B75 for QML: lighting effects with per-effect brightness, speed and
// a seven-colour palette, debounce, wireless sleep, base-layer key remapping
// and the knob / End-key layout switch.
//
// The config block (0x84) is read back from the keyboard. Key assignments and
// palettes have no read command, so — like the web studio's localStorage —
// they are kept in ~/.config/cheshub-plasma/ilovbee-b75.conf.
#pragma once

#include "../drivers/b75session.h"
#include "abstractdevice.h"

#include <QColor>
#include <QVariantList>

#include <memory>

namespace cheshub {

class B75Device : public AbstractDevice
{
    Q_OBJECT
    // Lighting
    Q_PROPERTY(int effect READ effect WRITE setEffect NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList effects READ effects CONSTANT) // [{ hw, name, speed, brightness, color }]
    Q_PROPERTY(int brightness READ brightness WRITE setBrightness NOTIFY settingsChanged) // 0..4
    Q_PROPERTY(int speed READ speed WRITE setSpeed NOTIFY settingsChanged) // 0..4
    Q_PROPERTY(bool colorful READ colorful WRITE setColorful NOTIFY settingsChanged)
    Q_PROPERTY(int colorSlot READ colorSlot WRITE setColorSlot NOTIFY settingsChanged) // 0..6
    Q_PROPERTY(QVariantList palette READ palette NOTIFY settingsChanged) // 7 x color
    // Settings
    Q_PROPERTY(int debounce READ debounce WRITE setDebounce NOTIFY settingsChanged)
    Q_PROPERTY(QStringList debounceOptions READ debounceOptions CONSTANT)
    Q_PROPERTY(int sleep READ sleep WRITE setSleep NOTIFY settingsChanged)
    Q_PROPERTY(QStringList sleepOptions READ sleepOptions CONSTANT)
    Q_PROPERTY(bool sleepApplies READ sleepApplies CONSTANT) // wireless only
    Q_PROPERTY(QString layout READ layout WRITE setLayout NOTIFY settingsChanged) // "knob" | "end"
    // Keys
    Q_PROPERTY(QVariantList keys READ keys NOTIFY settingsChanged) // [{ id, label, type, text }]
    Q_PROPERTY(QVariantList keyboardUsages READ keyboardUsages CONSTANT) // [{ usage, name }]
    Q_PROPERTY(QVariantList mediaActions READ mediaActions CONSTANT) // [{ code, name }]
    Q_PROPERTY(QVariantList mouseActions READ mouseActions CONSTANT) // [{ code, name }]
    Q_PROPERTY(QVariantList modifiers READ modifiers CONSTANT) // [{ bit, name }]

public:
    static bool matches(const hid::NodeInfo &node);

    explicit B75Device(const hid::NodeInfo &node, QObject *parent = nullptr);
    ~B75Device() override;

    QString kind() const override { return QStringLiteral("keyboard"); }
    QString name() const override { return QStringLiteral("iLovbee B75"); }
    QString link() const override;
    bool dirty() const override;

    int effect() const { return m_settings.effect; }
    void setEffect(int hw);
    QVariantList effects() const;
    int brightness() const { return current().brightness; }
    void setBrightness(int v);
    int speed() const { return current().speed; }
    void setSpeed(int v);
    bool colorful() const { return current().colorful; }
    void setColorful(bool on);
    int colorSlot() const { return current().slot; }
    void setColorSlot(int slot);
    QVariantList palette() const;
    Q_INVOKABLE void setPaletteColor(int slot, const QColor &color);

    int debounce() const { return m_settings.debounce; }
    void setDebounce(int index);
    QStringList debounceOptions() const;
    int sleep() const { return m_settings.sleep; }
    void setSleep(int index);
    QStringList sleepOptions() const;
    bool sleepApplies() const;
    QString layout() const;
    void setLayout(const QString &layout);

    QVariantList keys() const;
    QVariantList keyboardUsages() const;
    QVariantList mediaActions() const;
    QVariantList mouseActions() const;
    QVariantList modifiers() const;
    Q_INVOKABLE void assignKeyboard(int keyId, int modifiers, int usage);
    Q_INVOKABLE void assignMedia(int keyId, int usage);
    Q_INVOKABLE void assignMouse(int keyId, int code);
    Q_INVOKABLE void disableKey(int keyId);
    Q_INVOKABLE void resetKey(int keyId);
    Q_INVOKABLE void resetAllKeys();

    void sync() override;
    void apply() override;
    void loadDefaults() override;
    void factoryReset() override;

private:
    using Assignments = std::array<b75::Assignment, b75::KeyCount>;

    const b75::EffectState &current() const { return m_settings.effects[m_settings.effect]; }
    void editLighting(const std::function<void(b75::Settings &)> &change);
    void editKeys(const std::function<void(Assignments &, b75::Layout &)> &change);
    void changed(bool wasDirty);
    void assign(int keyId, const b75::Assignment &a);
    bool configDirty() const;
    bool paletteDirty() const { return m_settings.palettes != m_device.palettes; }
    bool matrixDirty() const { return m_assignments != m_writtenAssignments || m_layout != m_writtenLayout; }
    void loadStore();
    void saveStore() const;

    std::shared_ptr<B75Session> m_session;
    bool m_opened = false;
    b75::Settings m_settings; // UI
    b75::Settings m_device; // keyboard, as far as we know
    Bytes m_block; // last config block read or written
    Assignments m_assignments{};
    Assignments m_writtenAssignments{};
    b75::Layout m_layout = b75::Layout::Knob;
    b75::Layout m_writtenLayout = b75::Layout::Knob;
};

} // namespace cheshub
