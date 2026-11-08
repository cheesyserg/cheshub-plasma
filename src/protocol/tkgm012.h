// SPDX-License-Identifier: GPL-3.0-or-later
//
// TECKNET TK-GM012 wire protocol.
//
// Source of truth: cheshub/devices/tecknet-tk-gm012/docs/{PROTOCOL,REGISTERS}.md
// and driver.js. Offsets below are copied from there; see those documents for
// the evidence behind each field.
//
// Frame (33 bytes, report id 0):
//   [0] 0x00  [1] command  [2] 0x00  [3] 1 = write / 0 = read
//   [4] length/type byte   [5..31] payload (max 27 bytes)
//   [32] checksum = sum(bytes 5..31) & 0xFF
//
// Reply (32 bytes, no report id): [0] command echo, [1] status (0 = accepted),
// [4..] register contents for read commands.
#pragma once

#include "bytes.h"

#include <array>
#include <optional>
#include <string>

namespace cheshub::tkgm012 {

constexpr uint16_t WiredVendorId = 0x248A;
constexpr uint16_t WiredProductId = 0x5D2E;
constexpr uint16_t WirelessVendorId = 0x249A;
constexpr uint16_t WirelessProductId = 0x5C2F;
// Interface 2. The mouse/keyboard/consumer collections never answer.
constexpr uint16_t VendorUsagePage = 0xFF01;
constexpr uint16_t VendorUsage = 0x0010;

constexpr size_t FrameSize = 33;
constexpr size_t MaxPayload = 27;
constexpr int StageCount = 6;
constexpr int ButtonCount = 6;
constexpr int DpiMin = 50;
constexpr int DpiMax = 24000;

enum Command : uint8_t {
    Rate = 0x02,
    Dpi = 0x03,
    DpiColor = 0x04,
    Lighting = 0x05, // present in firmware; never sent to this model
    Sensor = 0x06, // present in firmware; never sent to this model
    Power = 0x07,
    KeyMacro = 0x08,
    KeyAll = 0x09,
    LightReset = 0x0E,
    Factory = 0x0F,
    // Reads are their own command numbers (write + 0x10), direction byte 0.
    Info = 0x10,
    KeysRead = 0x11,
    RateRead = 0x12,
    DpiRead = 0x13,
    DpiColorRead = 0x14,
    PowerRead = 0x17,
    Firmware = 0x20,
};

uint8_t checksum(const Bytes &frame);
Bytes buildFrame(uint8_t cmd, const Bytes &payload, uint8_t lengthByte, bool write);
inline Bytes buildRead(uint8_t cmd) { return buildFrame(cmd, {}, 0, false); }

// A reply counts only when it echoes the command and its status byte is 0
// (DriverComm::SetDeviceData @ 0x4B300).
bool isAck(const Bytes &reply, uint8_t cmd);

// ---------------------------------------------------------------- report rate
// RR = 1000 / hz.
constexpr std::array<int, 4> SupportedRates = {125, 250, 500, 1000};
uint8_t encodeRate(int hz); // unsupported -> 1000 Hz
std::optional<int> decodeRate(uint8_t code);

// ----------------------------------------------------------------------- DPI
// GetMouseDPIValue @ 0x1D740:
//   dpi <= 10000        -> dpi / 50
//   10000 < dpi < 13000 -> 200 + (dpi - 10000) / 100
//   dpi >= 13000        -> 221 + (dpi - 13000) / 1000
uint16_t encodeDpi(int dpi);
int decodeDpi(uint16_t code);
// Rounds to a value the codec represents exactly (step 50 / 100 / 1000).
// 12100..12900 is skipped: the vendor codec maps it onto codes 221..229, which
// also mean 13000..21000, so those values do not survive a read-back.
int snapDpi(int dpi);

// ----------------------------------------------------------------- power
// Sleep timeout is stored as minutes * 6.
constexpr std::array<int, 8> SleepOptions = {1, 2, 5, 6, 10, 15, 20, 30};
constexpr int SleepUnit = 6;
int nearestSleep(int minutes);

// ------------------------------------------------------------------ buttons
using KeyRecord = std::array<uint8_t, 3>;

struct Action {
    const char *id;
    const char *label;
    KeyRecord record;
};

// The 0x09 records that are known (PROTOCOL.md "Known records").
const std::array<Action, 19> &actions();
const Action *actionById(const std::string &id);
const Action *actionForRecord(const KeyRecord &record);
// Class label for a record outside the table (macro, keyboard shortcut, ...).
std::string recordClassName(const KeyRecord &record);

struct Button {
    int keyValue; // wire index
    const char *label;
};
// Visual order of the vendor XML; key_value 3/4 are swapped vs. visual order.
const std::array<Button, ButtonCount> &buttons();

// ------------------------------------------------------------------ settings

struct Settings {
    int rateHz = 1000;
    int stageCount = 6; // 1..6
    int activeStage = 1; // 0-based; default_dpi="2"
    std::array<int, StageCount> dpi = {800, 1600, 2400, 3200, 6400, 12000};
    std::array<Rgb, StageCount> colors = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFE1966, 0xFF4400};
    int sleepMinutes = 30; // XML sleep_light="180" == 30 min
    bool moveWake = false;
    uint8_t powerAux = 1; // constant marker, echoed back from the device
    // Indexed by key_value (wire order), not visual order.
    std::array<KeyRecord, ButtonCount> keys = {{
        {0x10, 0x01, 0x00}, // left
        {0x10, 0x02, 0x00}, // right
        {0x10, 0x04, 0x00}, // middle
        {0x10, 0x08, 0x00}, // backward (rear side button)
        {0x10, 0x10, 0x00}, // forward (front side button)
        {0x40, 0x01, 0x00}, // DPI loop
    }};

    bool operator==(const Settings &) const = default;
};

Settings factoryDefaults();

// Write frames. Each returns the full 33-byte frame ready for hidraw.
Bytes rateFrame(const Settings &s);
Bytes dpiFrame(const Settings &s);
Bytes dpiColorFrame(const Settings &s);
Bytes powerFrame(const Settings &s);
Bytes keysFrame(const Settings &s);
Bytes resetIndicatorFrame();
Bytes factoryResetFrame();

// Read-reply decoders. `reply` is the 32-byte input report (no report id).
// Each returns false and leaves `s` untouched when the reply is too short or
// carries values outside the documented range.
bool parseRate(const Bytes &reply, Settings &s);
bool parseDpi(const Bytes &reply, Settings &s);
bool parseDpiColors(const Bytes &reply, Settings &s);
bool parsePower(const Bytes &reply, Settings &s);
bool parseKeys(const Bytes &reply, Settings &s);

struct DeviceInfo {
    std::string deviceId; // "L93Q"
    std::optional<int> battery; // 1..100
    bool charging = false;
    int linkState = 0;
};
std::optional<DeviceInfo> parseInfo(const Bytes &reply);
// 0x20 reply: [4] minor BCD, [5] major BCD -> "3.26"
std::optional<std::string> parseFirmware(const Bytes &reply);

} // namespace cheshub::tkgm012
