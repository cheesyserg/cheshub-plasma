// SPDX-License-Identifier: GPL-3.0-or-later
#include "tkgm012.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace cheshub::tkgm012 {

uint8_t checksum(const Bytes &frame)
{
    unsigned sum = 0;
    for (size_t i = 5; i <= 31 && i < frame.size(); ++i)
        sum += frame[i];
    return sum & 0xFF;
}

Bytes buildFrame(uint8_t cmd, const Bytes &payload, uint8_t lengthByte, bool write)
{
    Bytes frame(FrameSize, 0);
    frame[0] = 0x00;
    frame[1] = cmd;
    frame[2] = 0x00;
    frame[3] = write ? 0x01 : 0x00;
    frame[4] = lengthByte;
    for (size_t i = 0; i < payload.size() && i < MaxPayload; ++i)
        frame[5 + i] = payload[i];
    frame[32] = checksum(frame);
    return frame;
}

bool isAck(const Bytes &reply, uint8_t cmd)
{
    return reply.size() > 1 && reply[0] == cmd && reply[1] == 0;
}

// ------------------------------------------------------------------- rate

uint8_t encodeRate(int hz)
{
    switch (hz) {
    case 125:
        return 0x08;
    case 250:
        return 0x04;
    case 500:
        return 0x02;
    default:
        return 0x01;
    }
}

std::optional<int> decodeRate(uint8_t code)
{
    switch (code) {
    case 0x08:
        return 125;
    case 0x04:
        return 250;
    case 0x02:
        return 500;
    case 0x01:
        return 1000;
    default:
        return std::nullopt;
    }
}

// -------------------------------------------------------------------- DPI

uint16_t encodeDpi(int dpi)
{
    const int v = std::clamp(dpi, DpiMin, DpiMax);
    if (v <= 10000)
        return uint16_t(v / 50);
    if (v < 13000)
        return uint16_t(200 + (v - 10000) / 100);
    return uint16_t(221 + (v - 13000) / 1000);
}

int decodeDpi(uint16_t code)
{
    if (code <= 200)
        return code * 50;
    if (code <= 220)
        return 10000 + (code - 200) * 100;
    return 13000 + (code - 221) * 1000;
}

int snapDpi(int dpi)
{
    int v = std::clamp(dpi, DpiMin, DpiMax);
    // Codes 221..229 are produced both by 12100..12900 and by 13000..21000,
    // so the mouse reads 12500 back as 17000. Keep out of that range.
    if (v > 12000 && v < 13000)
        v = v < 12500 ? 12000 : 13000;
    int step = v <= 10000 ? 50 : (v < 13000 ? 100 : 1000);
    int snapped = int(std::lround(double(v) / step)) * step;
    return decodeDpi(encodeDpi(std::clamp(snapped, DpiMin, DpiMax)));
}

// ------------------------------------------------------------------ power

int nearestSleep(int minutes)
{
    int best = SleepOptions[0];
    for (int option : SleepOptions) {
        if (std::abs(option - minutes) < std::abs(best - minutes))
            best = option;
    }
    return best;
}

// ---------------------------------------------------------------- buttons

const std::array<Action, 19> &actions()
{
    static const std::array<Action, 19> table = {{
        {"left", "Left click", {0x10, 0x01, 0x00}},
        {"right", "Right click", {0x10, 0x02, 0x00}},
        {"middle", "Middle click", {0x10, 0x04, 0x00}},
        {"back", "Backward", {0x10, 0x08, 0x00}},
        {"forward", "Forward", {0x10, 0x10, 0x00}},
        {"double", "Double click", {0x30, 0x32, 0x03}},
        {"scroll-up", "Scroll up", {0x20, 0x01, 0x00}},
        {"scroll-down", "Scroll down", {0x20, 0xFF, 0x00}},
        {"scroll-left", "Scroll left", {0x10, 0x00, 0xF5}},
        {"scroll-right", "Scroll right", {0x10, 0x00, 0xF6}},
        {"volume-up", "Volume up", {0x80, 0xE9, 0x00}},
        {"volume-down", "Volume down", {0x80, 0xEA, 0x00}},
        {"mute", "Mute", {0x80, 0xE2, 0x00}},
        {"play-pause", "Play / pause", {0x80, 0xCD, 0x00}},
        {"next", "Next track", {0x80, 0xB5, 0x00}},
        {"previous", "Previous track", {0x80, 0xB6, 0x00}},
        {"stop", "Stop", {0x80, 0xB7, 0x00}},
        {"dpi-loop", "DPI loop", {0x40, 0x01, 0x00}},
        {"disabled", "Disabled", {0x60, 0x00, 0x00}},
    }};
    return table;
}

const Action *actionById(const std::string &id)
{
    for (const Action &a : actions()) {
        if (id == a.id)
            return &a;
    }
    return nullptr;
}

const Action *actionForRecord(const KeyRecord &record)
{
    for (const Action &a : actions()) {
        if (a.record == record)
            return &a;
    }
    return nullptr;
}

std::string recordClassName(const KeyRecord &record)
{
    switch (record[0] & 0xF0) {
    case 0x10:
        return "Mouse button";
    case 0x20:
        return "Scroll";
    case 0x30:
        return "Double click / speed";
    case 0x40:
        return "DPI";
    case 0x60:
        return "Special";
    case 0x70:
        return "Keyboard shortcut";
    case 0x80:
        return "Media";
    case 0x90:
        return "Macro";
    default:
        return "Custom action";
    }
}

const std::array<Button, ButtonCount> &buttons()
{
    static const std::array<Button, ButtonCount> table = {{
        {0, "Left button"},
        {1, "Right button"},
        {2, "Middle button (wheel)"},
        {4, "Front side button"},
        {3, "Rear side button"},
        {5, "DPI button"},
    }};
    return table;
}

// --------------------------------------------------------------- settings

Settings factoryDefaults()
{
    return Settings{};
}

Bytes rateFrame(const Settings &s)
{
    return buildFrame(Rate, {encodeRate(s.rateHz)}, 1, true);
}

Bytes dpiFrame(const Settings &s)
{
    Bytes payload(25, 0);
    const int count = std::clamp(s.stageCount, 1, StageCount);
    const int active = std::clamp(s.activeStage, 0, count - 1);
    // SetMouseDPIData: (dpi_index << 4) | dpi_count
    payload[0] = uint8_t(((active & 0x0F) << 4) | (count & 0x0F));
    for (int i = 0; i < StageCount; ++i) {
        const uint16_t code = encodeDpi(s.dpi[i]);
        const size_t base = 1 + size_t(i) * 4;
        payload[base] = code & 0xFF; // X
        payload[base + 1] = code >> 8;
        payload[base + 2] = code & 0xFF; // Y carries the same code
        payload[base + 3] = code >> 8;
    }
    return buildFrame(Dpi, payload, 0x25, true);
}

Bytes dpiColorFrame(const Settings &s)
{
    Bytes payload;
    payload.reserve(18);
    for (Rgb c : s.colors) {
        payload.push_back(red(c));
        payload.push_back(green(c));
        payload.push_back(blue(c));
    }
    return buildFrame(DpiColor, payload, 0x12, true);
}

Bytes powerFrame(const Settings &s)
{
    const int wire = std::clamp(s.sleepMinutes * SleepUnit, 1, 255);
    // [sleep*6][move wake][aux = 1][close-light = 0]
    return buildFrame(Power, {uint8_t(wire), uint8_t(s.moveWake ? 1 : 0), uint8_t(s.powerAux ? 1 : 0), 0}, 4, true);
}

Bytes keysFrame(const Settings &s)
{
    Bytes payload;
    payload.reserve(18);
    for (const KeyRecord &r : s.keys)
        payload.insert(payload.end(), r.begin(), r.end());
    // The vendor sends 0x0F although 18 bytes follow; copied verbatim.
    return buildFrame(KeyAll, payload, 0x0F, true);
}

Bytes resetIndicatorFrame()
{
    return buildFrame(LightReset, {0x01}, 1, true);
}

Bytes factoryResetFrame()
{
    return buildFrame(Factory, {0x01, 0xFF}, 1, true);
}

// ---------------------------------------------------------------- decoders

bool parseRate(const Bytes &reply, Settings &s)
{
    if (reply.size() < 5)
        return false;
    auto hz = decodeRate(reply[4]);
    if (!hz)
        return false;
    s.rateHz = *hz;
    return true;
}

bool parseDpi(const Bytes &reply, Settings &s)
{
    if (reply.size() < 29)
        return false;
    const int packed = reply[4];
    const int count = packed & 0x0F;
    const int index = (packed >> 4) & 0x0F;
    if (count >= 1 && count <= StageCount)
        s.stageCount = count;
    if (index < StageCount)
        s.activeStage = std::min(index, s.stageCount - 1);
    for (int i = 0; i < StageCount; ++i) {
        const size_t base = 5 + size_t(i) * 4;
        s.dpi[i] = decodeDpi(uint16_t(reply[base] | (reply[base + 1] << 8)));
    }
    return true;
}

bool parseDpiColors(const Bytes &reply, Settings &s)
{
    if (reply.size() < 22)
        return false;
    for (int i = 0; i < StageCount; ++i) {
        const size_t base = 4 + size_t(i) * 3;
        s.colors[i] = makeRgb(reply[base], reply[base + 1], reply[base + 2]);
    }
    return true;
}

bool parsePower(const Bytes &reply, Settings &s)
{
    if (reply.size() < 7)
        return false;
    // [4] sleep*6  [5] move wake  [6] aux  [7] close-light
    if (reply[4] > 0)
        s.sleepMinutes = std::max(1, int(std::lround(reply[4] / double(SleepUnit))));
    s.moveWake = reply[5] == 1;
    s.powerAux = reply[6] == 0 ? 0 : 1;
    return true;
}

bool parseKeys(const Bytes &reply, Settings &s)
{
    if (reply.size() < 22)
        return false;
    for (int v = 0; v < ButtonCount; ++v) {
        const size_t base = 4 + size_t(v) * 3;
        s.keys[v] = {reply[base], reply[base + 1], reply[base + 2]};
    }
    return true;
}

std::optional<DeviceInfo> parseInfo(const Bytes &reply)
{
    if (reply.size() < 15 || reply[0] != Info)
        return std::nullopt;
    DeviceInfo info;
    for (size_t i = 4; i < 8; ++i) {
        if (reply[i] >= 0x20 && reply[i] < 0x7F)
            info.deviceId.push_back(char(reply[i]));
    }
    if (reply[13] >= 1 && reply[13] <= 100)
        info.battery = reply[13];
    info.charging = reply[12] == 1;
    info.linkState = reply[14];
    return info;
}

std::optional<std::string> parseFirmware(const Bytes &reply)
{
    if (reply.size() < 6 || reply[0] != Firmware)
        return std::nullopt;
    if (reply[4] == 0 && reply[5] == 0)
        return std::nullopt;
    char buf[16];
    // Vendor format "%X%02X" with language id 551 ("Mouse Firmware").
    std::snprintf(buf, sizeof buf, "%X.%02X", reply[5], reply[4]);
    return std::string(buf);
}

} // namespace cheshub::tkgm012
