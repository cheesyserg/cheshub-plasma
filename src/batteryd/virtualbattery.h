// SPDX-License-Identifier: GPL-3.0-or-later
// A virtual HID device (/dev/uhid) that carries one battery reading.
//
// The kernel's generic HID input driver turns a "Battery Strength" field
// (Generic Device Controls 0x06 / 0x20) into a power_supply with
// scope=Device. UPower picks that up and Plasma's Power & Battery applet
// lists it next to other peripheral batteries.
//
// UPower decides the icon (mouse / keyboard) from the input device next to
// the battery, so the descriptor also declares an idle mouse or keyboard that
// never sends anything. The udev rule hides that input device from libinput
// (LIBINPUT_IGNORE_DEVICE), so it never shows up as a second mouse/keyboard.
#pragma once

#include "../protocol/bytes.h"

#include <string>

namespace cheshub::batteryd {

enum class Kind { Mouse, Keyboard };

constexpr uint8_t BatteryReportId = 2;
// The udev rule matches input devices whose phys starts with this.
constexpr const char *Phys = "cheshub-batteryd";

// Report descriptor: idle mouse/keyboard on report 1, battery on report 2.
Bytes reportDescriptor(Kind kind);
// Input report 2: [id, level 0..100, bit 0 = charging].
Bytes batteryReport(int level, bool charging);

class VirtualBattery
{
public:
    VirtualBattery() = default;
    ~VirtualBattery();
    VirtualBattery(const VirtualBattery &) = delete;
    VirtualBattery &operator=(const VirtualBattery &) = delete;

    // name: shown in the applet ("TECKNET TK-GM012"). uniq: stable id used
    // for the power_supply name (hid-<uniq>-battery).
    bool create(Kind kind, const std::string &name, const std::string &uniq, uint16_t vendor, uint16_t product);
    void destroy();
    bool isCreated() const { return m_fd >= 0; }
    int fd() const { return m_fd; }
    const std::string &error() const { return m_error; }

    // Sends the reading now if the kernel has started the device, otherwise
    // as soon as it does.
    void setLevel(int level, bool charging);
    // Answers kernel requests (GET_REPORT for the battery, START/STOP).
    // Call when fd() is readable.
    void handleEvents();

private:
    void sendReport();

    int m_fd = -1;
    bool m_started = false;
    int m_level = -1;
    bool m_charging = false;
    std::string m_error;
};

} // namespace cheshub::batteryd
