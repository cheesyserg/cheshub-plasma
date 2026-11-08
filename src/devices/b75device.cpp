// SPDX-License-Identifier: GPL-3.0-or-later
#include "b75device.h"

#include <QHash>
#include <QRectF>
#include <QSettings>
#include <QVariantMap>

namespace cheshub {

using namespace b75;

namespace {

// HID keyboard usage names (driver.js HID_NAMES).
const QList<QPair<int, QString>> &usageNames()
{
    static const QList<QPair<int, QString>> table = [] {
        QList<QPair<int, QString>> t;
        for (int i = 0; i < 26; ++i)
            t.append({0x04 + i, QString(QChar(u'A' + i))});
        for (int i = 0; i < 9; ++i)
            t.append({0x1E + i, QString::number(i + 1)});
        t.append({0x27, QStringLiteral("0")});
        const QList<QPair<int, QString>> named = {
            {0x28, QStringLiteral("Enter")}, {0x29, QStringLiteral("Esc")}, {0x2A, QStringLiteral("Backspace")},
            {0x2B, QStringLiteral("Tab")}, {0x2C, QStringLiteral("Space")}, {0x2D, QStringLiteral("-")},
            {0x2E, QStringLiteral("=")}, {0x2F, QStringLiteral("[")}, {0x30, QStringLiteral("]")},
            {0x31, QStringLiteral("\\")}, {0x33, QStringLiteral(";")}, {0x34, QStringLiteral("'")},
            {0x35, QStringLiteral("`")}, {0x36, QStringLiteral(",")}, {0x37, QStringLiteral(".")},
            {0x38, QStringLiteral("/")}, {0x39, QStringLiteral("Caps Lock")},
        };
        t.append(named);
        for (int i = 0; i < 12; ++i)
            t.append({0x3A + i, QStringLiteral("F%1").arg(i + 1)});
        const QList<QPair<int, QString>> nav = {
            {0x46, QStringLiteral("Print Screen")}, {0x47, QStringLiteral("Scroll Lock")}, {0x48, QStringLiteral("Pause")},
            {0x49, QStringLiteral("Insert")}, {0x4A, QStringLiteral("Home")}, {0x4B, QStringLiteral("Page Up")},
            {0x4C, QStringLiteral("Delete")}, {0x4D, QStringLiteral("End")}, {0x4E, QStringLiteral("Page Down")},
            {0x4F, QStringLiteral("Right")}, {0x50, QStringLiteral("Left")}, {0x51, QStringLiteral("Down")},
            {0x52, QStringLiteral("Up")}, {0x53, QStringLiteral("Num Lock")}, {0x54, QStringLiteral("Num /")},
            {0x55, QStringLiteral("Num *")}, {0x56, QStringLiteral("Num -")}, {0x57, QStringLiteral("Num +")},
            {0x58, QStringLiteral("Num Enter")},
        };
        t.append(nav);
        for (int i = 0; i < 9; ++i)
            t.append({0x59 + i, QStringLiteral("Num %1").arg(i + 1)});
        t.append({0x62, QStringLiteral("Num 0")});
        t.append({0x63, QStringLiteral("Num .")});
        t.append({0x64, QStringLiteral("ISO \\")});
        t.append({0x65, QStringLiteral("Menu")});
        for (int i = 0; i < 12; ++i)
            t.append({0x68 + i, QStringLiteral("F%1").arg(i + 13)});
        const QList<QPair<int, QString>> mods = {
            {0xE0, QStringLiteral("Left Ctrl")}, {0xE1, QStringLiteral("Left Shift")}, {0xE2, QStringLiteral("Left Alt")},
            {0xE3, QStringLiteral("Left Meta")}, {0xE4, QStringLiteral("Right Ctrl")}, {0xE5, QStringLiteral("Right Shift")},
            {0xE6, QStringLiteral("Right Alt")}, {0xE7, QStringLiteral("Right Meta")},
        };
        t.append(mods);
        return t;
    }();
    return table;
}

QString usageName(int usage)
{
    for (const auto &p : usageNames()) {
        if (p.first == usage)
            return p.second;
    }
    return QStringLiteral("0x%1").arg(usage, 2, 16, QLatin1Char('0'));
}

const QList<QPair<int, QString>> &modifierNames()
{
    static const QList<QPair<int, QString>> table = {
        {0x01, QStringLiteral("Ctrl")}, {0x02, QStringLiteral("Shift")}, {0x04, QStringLiteral("Alt")},
        {0x08, QStringLiteral("Meta")}, {0x10, QStringLiteral("Right Ctrl")}, {0x20, QStringLiteral("Right Shift")},
        {0x40, QStringLiteral("Right Alt")}, {0x80, QStringLiteral("Right Meta")},
    };
    return table;
}

QString keyboardText(uint8_t mods, uint8_t usage)
{
    QStringList parts;
    for (const auto &m : modifierNames()) {
        if (mods & m.first)
            parts << m.second;
    }
    if (usage)
        parts << usageName(usage);
    return parts.join(QLatin1Char('+'));
}

QString describe(const Assignment &a)
{
    switch (a.type) {
    case Assignment::Type::Keyboard:
        return keyboardText(a.modifiers, a.usage);
    case Assignment::Type::Media:
        for (const NamedCode &c : b75::mediaActions()) {
            if (c.code == a.consumer)
                return QString::fromUtf8(c.name);
        }
        return QStringLiteral("Consumer 0x%1").arg(a.consumer, 4, 16, QLatin1Char('0'));
    case Assignment::Type::Mouse:
        for (const NamedCode &c : b75::mouseActions()) {
            if (c.code == a.raw)
                return QString::fromUtf8(c.name);
        }
        return QStringLiteral("Mouse");
    case Assignment::Type::Disabled:
        return QStringLiteral("Disabled");
    case Assignment::Type::Raw:
        return QStringLiteral("Code %1").arg(a.raw, 8, 16, QLatin1Char('0'));
    case Assignment::Type::Factory:
        break;
    }
    return QStringLiteral("Default");
}

QString typeName(Assignment::Type t)
{
    switch (t) {
    case Assignment::Type::Keyboard:
        return QStringLiteral("keyboard");
    case Assignment::Type::Media:
        return QStringLiteral("media");
    case Assignment::Type::Mouse:
        return QStringLiteral("mouse");
    case Assignment::Type::Disabled:
        return QStringLiteral("disabled");
    case Assignment::Type::Raw:
        return QStringLiteral("raw");
    case Assignment::Type::Factory:
        break;
    }
    return QStringLiteral("factory");
}

int keyIndex(int keyId)
{
    const KeyDef *k = keyById(keyId);
    return k ? int(k - b75::keys().data()) : -1;
}

QColor toColor(Rgb c)
{
    return QColor(red(c), green(c), blue(c));
}

} // namespace

// -------------------------------------------------------------------- setup

bool B75Device::matches(const hid::NodeInfo &node)
{
    auto link = B75Session::linkFor(node);
    return link && *link != B75Session::Link::Bluetooth;
}

B75Device::B75Device(const hid::NodeInfo &node, QObject *parent)
    : AbstractDevice(node, parent)
    , m_session(std::make_shared<B75Session>())
{
    loadStore();
    m_device = m_settings;
}

B75Device::~B75Device()
{
    m_worker->stop();
}

QString B75Device::link() const
{
    return B75Session::linkFor(m_node) == B75Session::Link::Wireless ? QStringLiteral("2.4 GHz receiver") : QStringLiteral("USB cable");
}

bool B75Device::sleepApplies() const
{
    return B75Session::linkFor(m_node) == B75Session::Link::Wireless;
}

bool B75Device::configDirty() const
{
    const Settings &a = m_settings;
    const Settings &b = m_device;
    return a.effect != b.effect || a.effects != b.effects || a.debounce != b.debounce || a.sleep != b.sleep;
}

bool B75Device::dirty() const
{
    return ready() && (configDirty() || paletteDirty() || matrixDirty());
}

void B75Device::changed(bool wasDirty)
{
    Q_EMIT settingsChanged();
    if (wasDirty != dirty())
        Q_EMIT dirtyChanged();
}

void B75Device::editLighting(const std::function<void(Settings &)> &change)
{
    const bool wasDirty = dirty();
    Settings next = m_settings;
    change(next);
    if (next == m_settings)
        return;
    m_settings = next;
    changed(wasDirty);
}

void B75Device::editKeys(const std::function<void(Assignments &, Layout &)> &change)
{
    const bool wasDirty = dirty();
    Assignments next = m_assignments;
    Layout layout = m_layout;
    change(next, layout);
    if (next == m_assignments && layout == m_layout)
        return;
    m_assignments = next;
    m_layout = layout;
    changed(wasDirty);
}

// ------------------------------------------------------------------ store

void B75Device::loadStore()
{
    QSettings store(QStringLiteral("cheshub-plasma"), QStringLiteral("ilovbee-b75"));
    m_layout = store.value(QStringLiteral("layout")).toString() == QLatin1String("end") ? Layout::EndKey : Layout::Knob;
    store.beginGroup(QStringLiteral("palettes"));
    for (const Effect &e : b75::effects()) {
        const QStringList colors = store.value(QStringLiteral("e%1").arg(e.hw)).toStringList();
        for (int i = 0; i < PaletteSize && i < int(colors.size()); ++i) {
            const QColor c(colors[i]);
            if (c.isValid())
                m_settings.palettes[e.hw][i] = makeRgb(c.red(), c.green(), c.blue());
        }
    }
    store.endGroup();
    store.beginGroup(QStringLiteral("keys"));
    for (size_t i = 0; i < b75::keys().size(); ++i) {
        const QStringList v = store.value(QStringLiteral("k%1").arg(b75::keys()[i].id)).toStringList();
        if (v.size() != 5)
            continue;
        Assignment a;
        a.type = Assignment::Type(qBound(0, v[0].toInt(), int(Assignment::Type::Raw)));
        a.modifiers = uint8_t(v[1].toUInt(nullptr, 16));
        a.usage = uint8_t(v[2].toUInt(nullptr, 16));
        a.consumer = uint16_t(v[3].toUInt(nullptr, 16));
        a.raw = v[4].toUInt(nullptr, 16);
        m_assignments[i] = a;
    }
    store.endGroup();
    // What was saved is what the keyboard was last given.
    m_writtenAssignments = m_assignments;
    m_writtenLayout = m_layout;
}

void B75Device::saveStore() const
{
    QSettings store(QStringLiteral("cheshub-plasma"), QStringLiteral("ilovbee-b75"));
    store.setValue(QStringLiteral("layout"), m_writtenLayout == Layout::EndKey ? QStringLiteral("end") : QStringLiteral("knob"));
    store.remove(QStringLiteral("palettes"));
    store.beginGroup(QStringLiteral("palettes"));
    for (const Effect &e : b75::effects()) {
        if (m_device.palettes[e.hw] == DefaultPalette)
            continue;
        QStringList colors;
        for (Rgb c : m_device.palettes[e.hw])
            colors << toColor(c).name();
        store.setValue(QStringLiteral("e%1").arg(e.hw), colors);
    }
    store.endGroup();
    store.remove(QStringLiteral("keys"));
    store.beginGroup(QStringLiteral("keys"));
    for (size_t i = 0; i < b75::keys().size(); ++i) {
        const Assignment &a = m_writtenAssignments[i];
        if (a.type == Assignment::Type::Factory)
            continue;
        store.setValue(QStringLiteral("k%1").arg(b75::keys()[i].id),
                       QStringList{QString::number(int(a.type)), QString::number(a.modifiers, 16), QString::number(a.usage, 16),
                                   QString::number(a.consumer, 16), QString::number(a.raw, 16)});
    }
    store.endGroup();
}

// --------------------------------------------------------------- lighting

void B75Device::setEffect(int hw)
{
    if (!effectByHw(hw))
        return;
    editLighting([hw](Settings &s) { s.effect = hw; });
}

QVariantList B75Device::effects() const
{
    QVariantList out;
    for (const Effect &e : b75::effects()) {
        QVariantMap m;
        m[QStringLiteral("hw")] = e.hw;
        m[QStringLiteral("name")] = QString::fromUtf8(e.name);
        m[QStringLiteral("speed")] = e.speed;
        m[QStringLiteral("brightness")] = e.brightness;
        m[QStringLiteral("color")] = e.color;
        out << m;
    }
    return out;
}

void B75Device::setBrightness(int v)
{
    editLighting([v](Settings &s) { s.effects[s.effect].brightness = qBound(0, v, int(LightLevels.size()) - 1); });
}

void B75Device::setSpeed(int v)
{
    editLighting([v](Settings &s) { s.effects[s.effect].speed = qBound(0, v, SpeedMax); });
}

void B75Device::setColorful(bool on)
{
    editLighting([on](Settings &s) { s.effects[s.effect].colorful = on; });
}

void B75Device::setColorSlot(int slot)
{
    editLighting([slot](Settings &s) {
        s.effects[s.effect].slot = qBound(0, slot, PaletteSize - 1);
        s.effects[s.effect].colorful = false;
    });
}

QVariantList B75Device::palette() const
{
    QVariantList out;
    for (Rgb c : m_settings.palettes[m_settings.effect])
        out << toColor(c);
    return out;
}

void B75Device::setPaletteColor(int slot, const QColor &color)
{
    if (slot < 0 || slot >= PaletteSize || !color.isValid())
        return;
    editLighting([slot, color](Settings &s) { s.palettes[s.effect][slot] = makeRgb(color.red(), color.green(), color.blue()); });
}

// --------------------------------------------------------------- settings

void B75Device::setDebounce(int index)
{
    editLighting([index](Settings &s) { s.debounce = qBound(0, index, int(b75::debounceOptions().size()) - 1); });
}

QStringList B75Device::debounceOptions() const
{
    QStringList out;
    for (const Option &o : b75::debounceOptions())
        out << QString::fromUtf8(o.name);
    return out;
}

void B75Device::setSleep(int index)
{
    editLighting([index](Settings &s) { s.sleep = qBound(0, index, int(b75::sleepOptions().size()) - 1); });
}

QStringList B75Device::sleepOptions() const
{
    QStringList out;
    for (const Option &o : b75::sleepOptions())
        out << QString::fromUtf8(o.name);
    return out;
}

QString B75Device::layout() const
{
    return m_layout == Layout::EndKey ? QStringLiteral("end") : QStringLiteral("knob");
}

void B75Device::setLayout(const QString &layout)
{
    const Layout l = layout == QLatin1String("end") ? Layout::EndKey : Layout::Knob;
    editKeys([l](Assignments &, Layout &current) { current = l; });
}

// ------------------------------------------------------------------- keys

// Keycap geometry for the line-art keyboard, ported from the web studio
// (driver.js GRID / keyRect): a 75% board on a 50-unit grid in an 879 x 399
// drawing. id -> column (u), row, width (u).
namespace {
struct Cap {
    double x, row, w;
};

const QHash<int, Cap> &capGrid()
{
    static const QHash<int, Cap> grid = [] {
        QHash<int, Cap> g;
        auto key = [&](int id, double x, double row, double w = 1) { g.insert(id, {x, row, w}); };
        auto run = [&](double row, int first, double x, int count) {
            for (int i = 0; i < count; ++i)
                key(first + i, x + i, row);
        };
        key(1, 0, 0);
        run(0, 2, 1.25, 4);
        run(0, 6, 5.5, 4);
        run(0, 10, 9.75, 4);
        key(14, 14, 0);
        key(81, 15.25, 0);
        run(1.25, 15, 0, 13);
        key(28, 13, 1.25, 2);
        key(29, 15.25, 1.25);
        key(30, 0, 2.25, 1.5);
        run(2.25, 31, 1.5, 12);
        key(43, 13.5, 2.25, 1.5);
        key(44, 15.25, 2.25);
        key(45, 0, 3.25, 1.75);
        run(3.25, 46, 1.75, 11);
        key(57, 12.75, 3.25, 2.25);
        key(80, 15.25, 3.25);
        key(58, 0, 4.25, 2.25);
        run(4.25, 59, 2.25, 10);
        key(69, 12.25, 4.25, 1.75);
        key(70, 14.25, 4.25);
        key(71, 0, 5.25, 1.25);
        key(72, 1.25, 5.25, 1.25);
        key(73, 2.5, 5.25, 1.25);
        key(74, 3.75, 5.25, 6.25);
        key(75, 10, 5.25, 1.25);
        key(76, 11.25, 5.25, 1.25);
        key(77, 13.25, 5.25);
        key(78, 14.25, 5.25);
        key(79, 15.25, 5.25);
        return g;
    }();
    return grid;
}

QRectF capRect(int id, Layout layout)
{
    constexpr double U = 50, X0 = 36, Y0 = 53, Gap = 3.2;
    if (layout == Layout::Knob) {
        // Knob in End's slot at keycap size; turn left / right above it.
        switch (id) {
        case 82:
            return QRectF(QPointF(800.6, 54.6), QPointF(847.4, 101.4));
        case 83:
            return QRectF(QPointF(800, 16), QPointF(821, 37));
        case 84:
            return QRectF(QPointF(827, 16), QPointF(848, 37));
        default:
            break;
        }
    }
    const auto it = capGrid().constFind(id);
    if (it == capGrid().constEnd())
        return {};
    return QRectF(X0 + it->x * U + Gap / 2, Y0 + it->row * U + Gap / 2, it->w * U - Gap, U - Gap);
}

QString capCaption(int id, const char *label, Layout layout)
{
    if (layout == Layout::Knob) {
        if (id == 82)
            return QString();
        if (id == 83)
            return QStringLiteral("\u21BA");
        if (id == 84)
            return QStringLiteral("\u21BB");
    }
    switch (id) {
    case 29:
        return QStringLiteral("Home");
    case 44:
        return QStringLiteral("PgUp");
    case 80:
        return QStringLiteral("PgDn");
    default:
        return QString::fromUtf8(label);
    }
}
} // namespace

QVariantList B75Device::keys() const
{
    const auto hidden = hiddenKeys(m_layout);
    QVariantList out;
    for (size_t i = 0; i < b75::keys().size(); ++i) {
        const KeyDef &k = b75::keys()[i];
        if (hidden.count(k.id))
            continue;
        const QRectF r = capRect(k.id, m_layout);
        QVariantMap m;
        m[QStringLiteral("id")] = k.id;
        m[QStringLiteral("label")] = QString::fromUtf8(k.label);
        m[QStringLiteral("caption")] = capCaption(k.id, k.label, m_layout);
        m[QStringLiteral("type")] = typeName(m_assignments[i].type);
        m[QStringLiteral("text")] = describe(m_assignments[i]);
        // Position in the 879 x 399 drawing.
        m[QStringLiteral("x")] = r.x();
        m[QStringLiteral("y")] = r.y();
        m[QStringLiteral("w")] = r.width();
        m[QStringLiteral("h")] = r.height();
        m[QStringLiteral("knob")] = m_layout == Layout::Knob && k.id >= 82 && k.id <= 84;
        out << m;
    }
    return out;
}

QVariantList B75Device::keyboardUsages() const
{
    QVariantList out;
    for (const auto &p : usageNames())
        out << QVariantMap{{QStringLiteral("usage"), p.first}, {QStringLiteral("name"), p.second}};
    return out;
}

QVariantList B75Device::mediaActions() const
{
    QVariantList out;
    for (const NamedCode &c : b75::mediaActions())
        out << QVariantMap{{QStringLiteral("code"), int(c.code)}, {QStringLiteral("name"), QString::fromUtf8(c.name)}};
    return out;
}

QVariantList B75Device::mouseActions() const
{
    QVariantList out;
    for (const NamedCode &c : b75::mouseActions())
        out << QVariantMap{{QStringLiteral("code"), int(c.code)}, {QStringLiteral("name"), QString::fromUtf8(c.name)}};
    return out;
}

QVariantList B75Device::modifiers() const
{
    QVariantList out;
    for (const auto &m : modifierNames())
        out << QVariantMap{{QStringLiteral("bit"), m.first}, {QStringLiteral("name"), m.second}};
    return out;
}

void B75Device::assign(int keyId, const Assignment &a)
{
    const int i = keyIndex(keyId);
    if (i < 0)
        return;
    editKeys([i, a](Assignments &as, Layout &) { as[size_t(i)] = a; });
}

void B75Device::assignKeyboard(int keyId, int modifiers, int usage)
{
    Assignment a;
    a.type = Assignment::Type::Keyboard;
    a.modifiers = uint8_t(modifiers);
    a.usage = uint8_t(usage);
    assign(keyId, a);
}

void B75Device::assignMedia(int keyId, int usage)
{
    Assignment a;
    a.type = Assignment::Type::Media;
    a.consumer = uint16_t(usage);
    assign(keyId, a);
}

void B75Device::assignMouse(int keyId, int code)
{
    Assignment a;
    a.type = Assignment::Type::Mouse;
    a.raw = uint32_t(code);
    assign(keyId, a);
}

void B75Device::disableKey(int keyId)
{
    Assignment a;
    a.type = Assignment::Type::Disabled;
    assign(keyId, a);
}

void B75Device::resetKey(int keyId)
{
    assign(keyId, Assignment{});
}

void B75Device::resetAllKeys()
{
    editKeys([](Assignments &as, Layout &) { as.fill(Assignment{}); });
}

// --------------------------------------------------------------- hardware

namespace {
struct SyncResult {
    bool opened = false;
    std::string error;
    std::string warning;
    std::optional<Bytes> block;
    std::optional<Power> power;
};

struct ApplyResult {
    bool ok = true;
    bool matrixWritten = false;
    bool configWritten = false;
    bool paletteWritten = false;
    std::string error;
    Bytes block;
};
} // namespace

void B75Device::sync()
{
    beginJob(QStringLiteral("Reading from the keyboard…"));
    auto session = m_session;
    const hid::NodeInfo node = m_node;
    const bool opened = m_opened;
    m_worker->run(
        this,
        [session, node, opened]() {
            SyncResult r;
            if (!opened && !session->open(node)) {
                r.error = session->error();
                return r;
            }
            r.opened = true;
            if (!session->keyboardOnline()) {
                r.error = "The keyboard is asleep — press any key on it, then reload this page.";
                return r;
            }
            if (session->link() == B75Session::Link::Wireless && !opened) {
                auto psd = session->pairingId();
                if (psd && !isB75PairingId(*psd))
                    r.warning = "This receiver is paired with a different iLovbee keyboard.";
            }
            r.block = session->readConfig();
            if (!r.block)
                r.error = session->error();
            r.power = session->readPower();
            return r;
        },
        [this](const SyncResult &r) {
            m_opened = r.opened;
            const bool wasDirty = dirty();
            if (r.block) {
                Settings next = m_device; // keeps the palettes
                decodeConfig(*r.block, next, m_block.empty() ? nullptr : &m_block);
                m_block = *r.block;
                m_device = next;
                m_settings = next;
                setReady(true);
            }
            if (r.power)
                setTelemetry(r.power->level.value_or(-1), r.power->charging);
            changed(wasDirty);
            endJob(QString::fromStdString(!r.error.empty() ? r.error : r.warning));
        });
}

void B75Device::apply()
{
    if (!dirty() || !m_opened)
        return;
    beginJob(QStringLiteral("Writing to the keyboard…"));
    auto session = m_session;
    const Settings target = m_settings;
    const Bytes block = m_block;
    const bool cfg = configDirty();
    const bool palette = paletteDirty();
    const bool matrix = matrixDirty();
    const Assignments assignments = m_assignments;
    const Layout layout = m_layout;
    m_worker->run(
        this,
        [session, target, block, cfg, palette, matrix, assignments, layout]() {
            ApplyResult r;
            r.block = block;
            auto fail = [&]() {
                r.ok = false;
                r.error = session->error();
                return r;
            };
            if (matrix) {
                if (!session->write(SetMatrix, 0, buildMatrix(assignments, layout)))
                    return fail();
                r.matrixWritten = true;
            }
            if (cfg) {
                // Patch the block read on connect: bytes the module does not
                // own are preserved, as the vendor tool does.
                const Bytes next = encodeConfig(block, target);
                if (!session->write(SetConfig, 0, next))
                    return fail();
                r.block = next;
                r.configWritten = true;
            }
            if (palette) {
                if (!session->write(SetRgbTable, 0, buildRgbTable(target)))
                    return fail();
                r.paletteWritten = true;
            }
            return r;
        },
        [this, target, assignments, layout](const ApplyResult &r) {
            const bool wasDirty = dirty();
            // Parts written before a failure are committed; the rest stay dirty.
            if (r.matrixWritten) {
                m_writtenAssignments = assignments;
                m_writtenLayout = layout;
            }
            if (r.configWritten) {
                m_block = r.block;
                m_device.effect = target.effect;
                m_device.effects = target.effects;
                m_device.debounce = target.debounce;
                m_device.sleep = target.sleep;
            }
            if (r.paletteWritten)
                m_device.palettes = target.palettes;
            saveStore();
            changed(wasDirty);
            const QString message = r.ok ? QStringLiteral("Saved to the keyboard") : QString::fromStdString(r.error);
            endJob(r.ok ? QString() : message);
            Q_EMIT applied(r.ok, message);
        });
}

void B75Device::loadDefaults()
{
    const bool wasDirty = dirty();
    const Settings defaults;
    m_settings = defaults;
    m_assignments.fill(Assignment{});
    changed(wasDirty);
}

void B75Device::factoryReset()
{
    if (!m_opened)
        return;
    beginJob(QStringLiteral("Restoring factory settings…"));
    auto session = m_session;
    const Bytes block = m_block;
    const Layout layout = m_layout;
    m_worker->run(
        this,
        [session, block, layout]() {
            const Settings defaults;
            Assignments none{};
            std::array<uint32_t, MatrixLength> unset;
            unset.fill(0xFF000000u);
            ApplyResult r;
            const bool ok = session->write(SetMatrix, 0, buildMatrix(none, layout))
                && session->write(SetConfig, 0, encodeConfig(block, defaults, true))
                && session->write(SetRgbTable, 0, buildRgbTable(defaults))
                && session->write(SetKeyRgb, 0, buildKeyRgb(unset));
            r.ok = ok;
            if (!ok)
                r.error = session->error();
            return r;
        },
        [this](const ApplyResult &r) {
            if (r.ok) {
                const Settings defaults;
                m_assignments.fill(Assignment{});
                m_writtenAssignments = m_assignments;
                m_device.palettes = defaults.palettes;
                m_settings.palettes = defaults.palettes;
                saveStore();
            }
            endJob(r.ok ? QString() : QString::fromStdString(r.error));
            Q_EMIT applied(r.ok, r.ok ? QStringLiteral("Factory settings restored") : QString::fromStdString(r.error));
            sync();
        });
}

} // namespace cheshub
