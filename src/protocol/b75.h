// SPDX-License-Identifier: GPL-3.0-or-later
//
// iLovbee B75 wire protocol (vendor class CDevG5KB, Fw=2, and CDev3632 for
// the 2.4 GHz receiver).
//
// Source of truth: cheshub/devices/ilovbee-b75/docs/PROTOCOL.md, driver.js and
// keymap.js. Every offset below is copied from there.
//
// Wired: 0x208-byte feature reports (including the report id) on the vendor
// collection (usage page 0xFF00/1 or 0xFF02/1-2):
//   [0] report id  [1] command  [2] sub  [3] 0  [4] packets  [5] index
//   [6..7] data length LE  [8..519] data (512 bytes per packet)
//
// Wireless (receiver 258A:01BB, usage page 0xFF02/2, report 0x13, 20 bytes):
//   [0] 0x13  [1] command  [2] packets  [3] index  [4] sub<<4 | len
//   [5..18] data (14 bytes)  [19] checksum = sum(bytes 0..18)
#pragma once

#include "bytes.h"

#include <array>
#include <optional>
#include <set>
#include <string>

namespace cheshub::b75 {

constexpr uint16_t VendorId = 0x258A;
constexpr uint16_t WiredProductId = 0x01E7;
constexpr uint16_t ReceiverProductId = 0x01BB;
// Over Bluetooth the board reports Logitech's vendor id; its vendor channel
// only sends a 1-byte status report, so it cannot be configured that way.
constexpr uint16_t BluetoothVendorId = 0x046D;
constexpr uint16_t BluetoothProductId = 0xB35A;

constexpr size_t FeatureReportBytes = 0x208; // HidP FeatureReportByteLength, incl. id
constexpr size_t FeaturePayload = FeatureReportBytes - 1;
constexpr size_t WiredChunk = 0x200;
constexpr int WiredPacketGapMs = 25; // vendor Sleep(0x19) before every packet

constexpr uint8_t WirelessReportId = 0x13;
constexpr size_t WirelessReportBytes = 20;
constexpr size_t WirelessChunk = 14;
constexpr int WirelessGapMs = 15;
constexpr int WirelessTimeoutMs = 4000;
constexpr uint8_t WirelessOnline = 0x07; // 1 byte: 0 = keyboard asleep / not linked

enum Command : uint8_t {
    SetMatrix = 0x03, // sub = layer, 126 x 4 bytes
    SetConfig = 0x04, // 0x80-byte config / lighting block
    SetMacro = 0x05,
    SetKeyRgb = 0x06, // sub = custom group, 3 planes x 126
    SetRgbTable = 0x0A, // 7 colours per effect, 21 bytes per effect id
    GetPairingId = 0x82,
    GetConfig = 0x84,
    GetPower = 0x87,
};

// Wired command -> wireless command (CDev3632).
std::optional<uint8_t> wirelessCommand(uint8_t wiredCmd);

// ------------------------------------------------------------ config block

constexpr size_t ConfigLength = 0x80;
namespace cfg {
constexpr size_t DebounceA = 0x02;
constexpr size_t DebounceB = 0x03;
constexpr size_t Zero = 0x05;
constexpr size_t Sleep = 0x09;
constexpr size_t ResetFlag = 37; // ResetItem=37,1
constexpr size_t Mode = 0x21;
constexpr size_t Color = 0x22;
constexpr size_t Bright = 0x23;
constexpr size_t Speed = 0x24;
constexpr size_t ModeTable = 0x53; // + hw*2: brightness, + hw*2 + 1: speed<<4 | colour
constexpr size_t SigA = 0x7E; // 0x5A
constexpr size_t SigB = 0x7F; // 0xA5
} // namespace cfg

bool hasSignature(const Bytes &block);

struct Effect {
    int hw; // id written to cfg[0x21]
    const char *name;
    bool speed;
    bool brightness;
    bool color; // takes a palette colour / colourful
};
// LedOpt1..19 from KB.ini, LedMask 0x2100 applied.
const std::array<Effect, 17> &effects();
const Effect *effectByHw(int hw);
constexpr int CustomEffect = 19;
constexpr int EffectSlots = 20; // hw ids 0..19

constexpr std::array<uint8_t, 5> LightLevels = {0, 5, 10, 15, 20}; // LightHW
constexpr int DefaultBrightness = 4;
constexpr int SpeedMax = 4;
constexpr int DefaultSpeed = 3;
constexpr int PaletteSize = 7;
constexpr int Colorful = 7; // colour nibble 7 = colourful
constexpr std::array<Rgb, PaletteSize> DefaultPalette = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF, 0xFFFFFF};

struct Option {
    uint8_t hw;
    const char *name;
};
const std::array<Option, 2> &debounceOptions(); // Office 0x00, Gaming 0x04
const std::array<Option, 5> &sleepOptions(); // 5/10/20/30 min (30 s units), never

struct EffectState {
    int brightness = DefaultBrightness; // index into LightLevels
    int speed = DefaultSpeed; // 0..4
    int slot = 0; // palette slot 0..6
    bool colorful = true;
    bool operator==(const EffectState &) const = default;
};

struct Settings {
    int effect = 9; // the studio's factory pick
    std::array<EffectState, EffectSlots> effects{};
    std::array<std::array<Rgb, PaletteSize>, EffectSlots> palettes = filledPalettes();
    int debounce = 0; // index into debounceOptions()
    int sleep = 1; // index into sleepOptions() (10 min)

    static std::array<std::array<Rgb, PaletteSize>, EffectSlots> filledPalettes()
    {
        std::array<std::array<Rgb, PaletteSize>, EffectSlots> p;
        p.fill(DefaultPalette);
        return p;
    }
    bool operator==(const Settings &) const = default;
};

// Brightness may come back on the vendor scale (0/5/10/15/20) or on a firmware
// scale after an Fn hotkey; map to the nearest of the five levels.
int brightnessIndex(uint8_t raw);

// Port of driver.js decodeCfg(). `previous` is the last block read (or
// written); when given, hotkey changes to either the active-effect bytes or the
// per-effect table win, whichever moved. Palettes are not part of the block.
void decodeConfig(const Bytes &block, Settings &s, const Bytes *previous = nullptr);
// FillCfg_LowDaly: patches only the bytes the vendor tool owns.
Bytes encodeConfig(const Bytes &block, const Settings &s, bool factoryReset = false);

constexpr size_t RgbTableLength = 20 * 21;
Bytes buildRgbTable(const Settings &s);

// ------------------------------------------------------------- key matrix

constexpr size_t MatrixLength = 0x7E; // 126 entries == LED count
constexpr int KeyCount = 84;

struct KeyDef {
    int id; // Kn in KB.ini
    const char *label;
    int matrix; // matrix index == per-key LED index
    uint32_t factory; // 4-byte hardware code, big-endian
};
const std::array<KeyDef, KeyCount> &keys();
const KeyDef *keyById(int id);

enum class Layout { Knob, EndKey };
// Keys a layout does not have; they keep their factory code.
std::set<int> hiddenKeys(Layout layout);

struct Assignment {
    enum class Type { Factory, Keyboard, Media, Mouse, Disabled, Raw };
    Type type = Type::Factory;
    uint8_t modifiers = 0; // 1 LCtrl 2 LShift 4 LAlt 8 LWin, 0x10..0x80 right
    uint8_t usage = 0; // HID keyboard usage
    uint16_t consumer = 0; // consumer usage for Media
    uint32_t raw = 0; // Mouse / Raw: 4-byte code, big-endian

    bool operator==(const Assignment &) const = default;
};

struct NamedCode {
    uint32_t code;
    const char *name;
};
const std::array<NamedCode, 17> &mediaActions(); // consumer usages
const std::array<NamedCode, 7> &mouseActions(); // 4-byte codes

// Keyboard entry [00, mods, 00, usage]; a modifier usage E0-E7 is moved into
// the modifier bits because the firmware ignores it in the usage byte.
std::array<uint8_t, 4> keyCode(uint8_t modifiers, uint8_t usage);
std::array<uint8_t, 4> encodeAssignment(const KeyDef &key, const Assignment &a);
// assignments is indexed like keys(); Factory entries and keys the layout
// hides use their factory code. Matrix slots without a key stay zero, as in the
// vendor's FillMatrix.
Bytes buildMatrix(const std::array<Assignment, KeyCount> &assignments, Layout layout);

// Per-key colour planes for the Custom effect: [R x126][G x126][B x126].
// colors: matrix index -> colour, 0xFF000000 marks "unset".
Bytes buildKeyRgb(const std::array<uint32_t, MatrixLength> &colors);

// ---------------------------------------------------------- wired frames

Bytes wiredFrame(uint8_t reportId, uint8_t cmd, uint8_t sub, uint8_t packets, uint8_t index,
                 const uint8_t *data, size_t length, size_t featureBytes = FeaturePayload);
std::vector<Bytes> wiredWrite(uint8_t reportId, uint8_t cmd, uint8_t sub, const Bytes &data,
                              size_t featureBytes = FeaturePayload);
Bytes wiredReadRequest(uint8_t reportId, uint8_t cmd, uint8_t sub, uint8_t packets, uint8_t index,
                       uint16_t size, size_t featureBytes = FeaturePayload);
// HIDIOCGFEATURE reply (report id at [0]) -> `size` data bytes, or nothing when
// the reply does not echo `cmd`.
std::optional<Bytes> wiredReplyData(const Bytes &reply, uint8_t cmd, size_t size);

// -------------------------------------------------------- wireless frames

Bytes wirelessFrame(uint8_t cmd, uint8_t total, uint8_t index, uint8_t sub, const uint8_t *data, size_t length);
std::vector<Bytes> wirelessWrite(uint8_t wirelessCmd, uint8_t sub, const Bytes &data);
inline Bytes wirelessReadRequest(uint8_t wirelessCmd) { return wirelessFrame(wirelessCmd, 1, 0, 0, nullptr, 0); }

// Collects the reply packets of one wireless read.
class WirelessReader
{
public:
    WirelessReader(uint8_t cmd, size_t length);
    enum class Result { Ignored, Pending, Done, Error };
    // `report` is one input report including its id byte.
    Result feed(const Bytes &report);
    const Bytes &data() const { return m_data; }

private:
    uint8_t m_cmd;
    Bytes m_data;
    std::set<int> m_seen;
};

// Acknowledgement for one written wireless packet.
enum class WirelessAck { Ignored, Ok, Resend, Error };
WirelessAck wirelessAck(const Bytes &report, uint8_t cmd);

// ------------------------------------------------------------------ misc

struct Power {
    std::optional<int> level;
    bool charging = false;
};
std::optional<Power> parsePower(const Bytes &data);

// KB.ini Psd=a,0,0,0,0,12 identifies the B75 behind a receiver.
bool isB75PairingId(const Bytes &data);

} // namespace cheshub::b75
