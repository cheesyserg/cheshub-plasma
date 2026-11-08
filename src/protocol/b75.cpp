// SPDX-License-Identifier: GPL-3.0-or-later
#include "b75.h"

#include <algorithm>
#include <cmath>

namespace cheshub::b75 {

std::optional<uint8_t> wirelessCommand(uint8_t wiredCmd)
{
    switch (wiredCmd) {
    case SetMatrix:
        return 0x01;
    case SetKeyRgb:
        return 0x02;
    case SetMacro:
        return 0x03;
    case SetConfig:
        return 0x04;
    case SetRgbTable:
        return 0x09;
    case GetPairingId:
        return 0x05;
    case GetConfig:
        return 0x44;
    case GetPower:
        return 0x4A;
    case WirelessOnline:
        return WirelessOnline;
    default:
        return std::nullopt;
    }
}

// ----------------------------------------------------------- config block

bool hasSignature(const Bytes &block)
{
    return block.size() >= ConfigLength && block[cfg::SigA] == 0x5A && block[cfg::SigB] == 0xA5;
}

const std::array<Effect, 17> &effects()
{
    // hw, name, speed, brightness, colour
    static const std::array<Effect, 17> table = {{
        {1, "Steady", false, true, true},
        {2, "Breathing", true, true, true},
        {3, "Rainbow", true, true, false},
        {4, "Flash Away", true, true, true},
        {5, "Raindrops", true, true, true},
        {6, "Rainbow Wheel", true, true, true},
        {7, "Ripples Shining", true, true, true},
        {8, "Stars Twinkle", true, true, true},
        {9, "Retro Snake", true, true, true},
        {10, "Neon Stream", true, true, true},
        {11, "Reaction", true, true, true},
        {12, "Sine Wave", true, true, true},
        {13, "Rotating Windmill", true, true, false},
        {14, "Colorful Waterfall", true, true, false},
        {15, "Blossoming", true, true, false},
        {CustomEffect, "Custom (per key)", false, false, false},
        {0, "Off", false, false, false},
    }};
    return table;
}

const Effect *effectByHw(int hw)
{
    for (const Effect &e : effects()) {
        if (e.hw == hw)
            return &e;
    }
    return nullptr;
}

const std::array<Option, 2> &debounceOptions()
{
    // DebounceUI=1,2 / DebounceHW=0,4. Which name is which index is inferred.
    static const std::array<Option, 2> table = {{{0x00, "Office"}, {0x04, "Gaming"}}};
    return table;
}

const std::array<Option, 5> &sleepOptions()
{
    static const std::array<Option, 5> table = {{
        {10, "5 minutes"},
        {20, "10 minutes"},
        {40, "20 minutes"},
        {60, "30 minutes"},
        {0, "Never"},
    }};
    return table;
}

int brightnessIndex(uint8_t raw)
{
    for (size_t i = 0; i < LightLevels.size(); ++i) {
        if (LightLevels[i] == raw)
            return int(i);
    }
    const int top = LightLevels.back();
    const double scale = raw <= top ? top : (raw <= 100 ? 100 : 255);
    const int last = int(LightLevels.size()) - 1;
    return std::clamp(int(std::lround(raw / scale * last)), 0, last);
}

void decodeConfig(const Bytes &block, Settings &s, const Bytes *previous)
{
    if (block.size() < ConfigLength)
        return;
    if (previous && previous->size() < ConfigLength)
        previous = nullptr;

    if (effectByHw(block[cfg::Mode]))
        s.effect = block[cfg::Mode];

    // A table that was never written is all zeros; then 0 means "unset",
    // not "off". Once anything is in it, 0 really is brightness off.
    bool tableLive = false;
    for (const Effect &e : effects()) {
        if (e.hw && (block[cfg::ModeTable + e.hw * 2] || block[cfg::ModeTable + e.hw * 2 + 1]))
            tableLive = true;
    }

    for (const Effect &e : effects()) {
        if (!e.hw)
            continue;
        const uint8_t b = block[cfg::ModeTable + e.hw * 2];
        const uint8_t sc = block[cfg::ModeTable + e.hw * 2 + 1];
        EffectState &st = s.effects[e.hw];
        if (b || tableLive)
            st.brightness = brightnessIndex(b);
        if (b == 0 && sc == 0 && !tableLive)
            continue;
        if ((sc >> 4) <= SpeedMax)
            st.speed = sc >> 4;
        const int slot = sc & 0x0F;
        if (slot == Colorful) {
            st.colorful = true;
        } else if (slot < PaletteSize) {
            st.colorful = false;
            st.slot = slot;
        }
    }

    // The active effect also has its own brightness / speed bytes. Fn hotkeys
    // may update either copy; on later reads, the copy that moved wins.
    if (s.effect) {
        EffectState &cur = s.effects[s.effect];
        const size_t tb = cfg::ModeTable + size_t(s.effect) * 2;
        auto changed = [&](size_t i) { return !previous || (*previous)[i] != block[i]; };
        auto pickActive = [&](size_t activeIdx, size_t tableIdx) {
            return !previous || (changed(activeIdx) && !changed(tableIdx));
        };
        const uint8_t activeBright = block[cfg::Bright];
        if ((activeBright || tableLive) && pickActive(cfg::Bright, tb))
            cur.brightness = brightnessIndex(activeBright);
        const uint8_t activeSpeed = block[cfg::Speed];
        if (tableLive && activeSpeed <= SpeedMax && pickActive(cfg::Speed, tb + 1))
            cur.speed = activeSpeed;
    }

    for (size_t i = 0; i < debounceOptions().size(); ++i) {
        if (debounceOptions()[i].hw == block[cfg::DebounceA])
            s.debounce = int(i);
    }
    for (size_t i = 0; i < sleepOptions().size(); ++i) {
        if (sleepOptions()[i].hw == block[cfg::Sleep])
            s.sleep = int(i);
    }
}

Bytes encodeConfig(const Bytes &block, const Settings &s, bool factoryReset)
{
    Bytes out = block;
    out.resize(ConfigLength, 0);
    for (const Effect &e : effects()) {
        if (!e.hw)
            continue;
        const EffectState &st = s.effects[e.hw];
        const int slot = st.colorful ? Colorful : (st.slot & 0x0F);
        out[cfg::ModeTable + e.hw * 2] = LightLevels[std::clamp(st.brightness, 0, 4)];
        out[cfg::ModeTable + e.hw * 2 + 1] = uint8_t(((std::clamp(st.speed, 0, SpeedMax) & 0x0F) << 4) | slot);
    }
    out[cfg::Mode] = uint8_t(s.effect);
    if (s.effect) {
        const EffectState &cur = s.effects[s.effect];
        out[cfg::Color] = uint8_t(cur.colorful ? Colorful : cur.slot);
        out[cfg::Bright] = LightLevels[std::clamp(cur.brightness, 0, 4)];
        out[cfg::Speed] = uint8_t(std::clamp(cur.speed, 0, SpeedMax));
    }
    const uint8_t debounce = debounceOptions()[std::clamp(s.debounce, 0, int(debounceOptions().size()) - 1)].hw;
    out[cfg::DebounceA] = debounce;
    out[cfg::DebounceB] = debounce;
    out[cfg::Sleep] = sleepOptions()[std::clamp(s.sleep, 0, int(sleepOptions().size()) - 1)].hw;
    out[cfg::Zero] = 0;
    if (factoryReset)
        out[cfg::ResetFlag] = 1;
    out[cfg::SigA] = 0x5A;
    out[cfg::SigB] = 0xA5;
    return out;
}

Bytes buildRgbTable(const Settings &s)
{
    Bytes tab(RgbTableLength, 0);
    for (const Effect &e : effects()) {
        if (!e.hw)
            continue;
        for (int slot = 0; slot < PaletteSize; ++slot) {
            const Rgb c = s.palettes[e.hw][slot];
            const size_t base = size_t(e.hw) * 21 + size_t(slot) * 3;
            tab[base] = red(c);
            tab[base + 1] = green(c);
            tab[base + 2] = blue(c);
        }
    }
    return tab;
}

// -------------------------------------------------------------- key matrix

const KeyDef *keyById(int id)
{
    for (const KeyDef &k : keys()) {
        if (k.id == id)
            return &k;
    }
    return nullptr;
}

std::set<int> hiddenKeys(Layout layout)
{
    // knob: rotary knob in place of End (retail B75). End key: no knob.
    return layout == Layout::Knob ? std::set<int>{81} : std::set<int>{82, 83, 84};
}

const std::array<NamedCode, 17> &mediaActions()
{
    static const std::array<NamedCode, 17> table = {{
        {0x00CD, "Play / pause"},
        {0x00B7, "Stop"},
        {0x00B6, "Previous track"},
        {0x00B5, "Next track"},
        {0x00E9, "Volume up"},
        {0x00EA, "Volume down"},
        {0x00E2, "Mute"},
        {0x0183, "Media player"},
        {0x018A, "Email"},
        {0x0192, "Calculator"},
        {0x0194, "File explorer"},
        {0x0221, "Search"},
        {0x0223, "Browser home"},
        {0x0227, "Refresh"},
        {0x022A, "Favorites"},
        {0x006F, "Screen brightness up"},
        {0x0070, "Screen brightness down"},
    }};
    return table;
}

const std::array<NamedCode, 7> &mouseActions()
{
    static const std::array<NamedCode, 7> table = {{
        {0x01010100, "Left mouse button"},
        {0x01010200, "Right mouse button"},
        {0x01010300, "Middle mouse button"},
        {0x01010400, "Mouse back"},
        {0x01010500, "Mouse forward"},
        {0x01080000, "Scroll up"},
        {0x01090000, "Scroll down"},
    }};
    return table;
}

static std::array<uint8_t, 4> beBytes(uint32_t v)
{
    return {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)};
}

std::array<uint8_t, 4> keyCode(uint8_t modifiers, uint8_t usage)
{
    uint8_t m = modifiers;
    uint8_t u = usage;
    if (u >= 0xE0 && u <= 0xE7) {
        m |= uint8_t(1u << (u - 0xE0));
        u = 0;
    }
    return {0x00, m, 0x00, u};
}

static std::array<uint8_t, 4> factoryCode(const KeyDef &key)
{
    auto c = beBytes(key.factory);
    return c[0] == 0x00 ? keyCode(c[1], c[3]) : c;
}

std::array<uint8_t, 4> encodeAssignment(const KeyDef &key, const Assignment &a)
{
    switch (a.type) {
    case Assignment::Type::Keyboard:
        return keyCode(a.modifiers, a.usage);
    case Assignment::Type::Media:
        return {0x02, 0x00, uint8_t(a.consumer >> 8), uint8_t(a.consumer)};
    case Assignment::Type::Mouse:
    case Assignment::Type::Raw:
        return beBytes(a.raw);
    case Assignment::Type::Disabled:
        return {0, 0, 0, 0};
    case Assignment::Type::Factory:
        break;
    }
    return factoryCode(key);
}

Bytes buildMatrix(const std::array<Assignment, KeyCount> &assignments, Layout layout)
{
    const auto hidden = hiddenKeys(layout);
    Bytes matrix(MatrixLength * 4, 0);
    for (size_t i = 0; i < keys().size(); ++i) {
        const KeyDef &key = keys()[i];
        if (size_t(key.matrix) >= MatrixLength)
            continue;
        const Assignment a = hidden.count(key.id) ? Assignment{} : assignments[i];
        const auto code = encodeAssignment(key, a);
        std::copy(code.begin(), code.end(), matrix.begin() + key.matrix * 4);
    }
    return matrix;
}

Bytes buildKeyRgb(const std::array<uint32_t, MatrixLength> &colors)
{
    Bytes planes(MatrixLength * 3, 0);
    for (size_t i = 0; i < MatrixLength; ++i) {
        if (colors[i] & 0xFF000000u)
            continue;
        planes[i] = red(colors[i]);
        planes[MatrixLength + i] = green(colors[i]);
        planes[MatrixLength * 2 + i] = blue(colors[i]);
    }
    return planes;
}

// ------------------------------------------------------------ wired frames

Bytes wiredFrame(uint8_t reportId, uint8_t cmd, uint8_t sub, uint8_t packets, uint8_t index,
                 const uint8_t *data, size_t length, size_t featureBytes)
{
    Bytes out(1 + featureBytes, 0);
    out[0] = reportId;
    out[1] = cmd;
    out[2] = sub;
    out[3] = 0;
    out[4] = packets;
    out[5] = index;
    out[6] = length & 0xFF;
    out[7] = (length >> 8) & 0xFF;
    const size_t n = std::min(length, featureBytes - 7);
    if (data && n)
        std::copy(data, data + n, out.begin() + 8);
    return out;
}

std::vector<Bytes> wiredWrite(uint8_t reportId, uint8_t cmd, uint8_t sub, const Bytes &data, size_t featureBytes)
{
    std::vector<Bytes> frames;
    const size_t packets = std::max<size_t>(1, (data.size() + WiredChunk - 1) / WiredChunk);
    for (size_t i = 0; i < packets; ++i) {
        const size_t begin = i * WiredChunk;
        const size_t len = begin < data.size() ? std::min(WiredChunk, data.size() - begin) : 0;
        frames.push_back(wiredFrame(reportId, cmd, sub, uint8_t(packets), uint8_t(i),
                                    len ? data.data() + begin : nullptr, len, featureBytes));
    }
    return frames;
}

Bytes wiredReadRequest(uint8_t reportId, uint8_t cmd, uint8_t sub, uint8_t packets, uint8_t index,
                       uint16_t size, size_t featureBytes)
{
    // Same header as a write, no data, length field = bytes wanted.
    Bytes out = wiredFrame(reportId, cmd, sub, packets, index, nullptr, 0, featureBytes);
    out[6] = size & 0xFF;
    out[7] = size >> 8;
    return out;
}

std::optional<Bytes> wiredReplyData(const Bytes &reply, uint8_t cmd, size_t size)
{
    // With the report id at [0] the command is at [1]; accept either layout
    // like the web driver does.
    size_t off;
    if (reply.size() > 1 && reply[1] == cmd)
        off = 1;
    else if (!reply.empty() && reply[0] == cmd)
        off = 0;
    else
        return std::nullopt;
    const size_t begin = off + 7;
    if (reply.size() < begin + size)
        return std::nullopt;
    return Bytes(reply.begin() + begin, reply.begin() + begin + size);
}

// --------------------------------------------------------- wireless frames

Bytes wirelessFrame(uint8_t cmd, uint8_t total, uint8_t index, uint8_t sub, const uint8_t *data, size_t length)
{
    Bytes b(WirelessReportBytes, 0);
    const size_t n = std::min(length, WirelessChunk);
    b[0] = WirelessReportId;
    b[1] = cmd;
    b[2] = total;
    b[3] = index;
    b[4] = uint8_t(((sub & 0x0F) << 4) | (n & 0x0F));
    if (data && n)
        std::copy(data, data + n, b.begin() + 5);
    unsigned sum = 0;
    for (size_t i = 0; i < 19; ++i)
        sum += b[i];
    b[19] = sum & 0xFF;
    return b;
}

std::vector<Bytes> wirelessWrite(uint8_t wirelessCmd, uint8_t sub, const Bytes &data)
{
    // Macro pages (cmd 0x03, one transfer per 512-byte page) are not written
    // by this module, so every block goes out as one transfer.
    std::vector<Bytes> frames;
    const size_t total = std::max<size_t>(1, (data.size() + WirelessChunk - 1) / WirelessChunk);
    for (size_t i = 0; i < total; ++i) {
        const size_t begin = i * WirelessChunk;
        const size_t len = begin < data.size() ? std::min(WirelessChunk, data.size() - begin) : 0;
        frames.push_back(wirelessFrame(wirelessCmd, uint8_t(total), uint8_t(i), sub,
                                       len ? data.data() + begin : nullptr, len));
    }
    return frames;
}

WirelessReader::WirelessReader(uint8_t cmd, size_t length)
    : m_cmd(cmd)
    , m_data(length, 0)
{
}

WirelessReader::Result WirelessReader::feed(const Bytes &report)
{
    if (report.empty())
        return Result::Ignored;
    if (report[0] == 0xFF)
        return Result::Error;
    if (report[0] != WirelessReportId || report.size() < WirelessReportBytes)
        return Result::Ignored;
    if ((report[1] & 0x7F) != m_cmd)
        return Result::Ignored;
    if (report[1] & 0x80)
        return Result::Error; // keyboard rejected the request
    const int total = report[2] & 0x7F;
    const int index = report[3];
    if (!m_seen.count(index)) {
        m_seen.insert(index);
        const size_t offset = size_t(index) * WirelessChunk;
        if (offset < m_data.size()) {
            const size_t n = std::min({size_t(report[4] & 0x0F), WirelessChunk, m_data.size() - offset});
            std::copy(report.begin() + 5, report.begin() + 5 + n, m_data.begin() + offset);
        }
    }
    return index >= total - 1 ? Result::Done : Result::Pending;
}

WirelessAck wirelessAck(const Bytes &report, uint8_t cmd)
{
    if (report.empty())
        return WirelessAck::Ignored;
    if (report[0] == 0xFF)
        return WirelessAck::Error;
    if (report[0] != WirelessReportId || report.size() < 3 || (report[1] & 0x7F) != cmd)
        return WirelessAck::Ignored;
    return (report[2] & 0x80) ? WirelessAck::Resend : WirelessAck::Ok;
}

// -------------------------------------------------------------------- misc

std::optional<Power> parsePower(const Bytes &data)
{
    if (data.size() < 2)
        return std::nullopt;
    Power p;
    if (data[0] <= 100)
        p.level = data[0];
    p.charging = data[1] != 0;
    return p;
}

bool isB75PairingId(const Bytes &data)
{
    static const Bytes expected = {0x0A, 0, 0, 0, 0, 0x12};
    return data.size() >= expected.size() && std::equal(expected.begin(), expected.end(), data.begin());
}

} // namespace cheshub::b75
