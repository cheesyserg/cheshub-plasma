// SPDX-License-Identifier: GPL-3.0-or-later
// Blocking TK-GM012 session over hidraw. Mirrors the transaction rules of the
// vendor's DriverComm::SetDeviceData and of driver.js:
//   * every connection starts with the 0x10 info read (wakes the 2.4G link)
//   * a reply only counts if it echoes the command with status 0
//   * first wait 600 ms, resend once and wait 1500 ms; a write that is never
//     acknowledged is reported as "silent", not as a failure.
// Not thread-safe; the Qt layer runs it on a worker thread.
#pragma once

#include "../hid/hidraw.h"
#include "../protocol/tkgm012.h"

#include <optional>
#include <string>

namespace cheshub {

struct Result {
    bool ok = false;
    bool silent = false; // accepted by the OS, device did not acknowledge
    std::string error;
    Bytes reply;
};

class TkGm012Session
{
public:
    static bool matches(const hid::NodeInfo &node);
    static bool isWireless(const hid::NodeInfo &node);

    bool open(const hid::NodeInfo &node);
    void close() { m_dev.close(); }
    const std::string &error() const { return m_error; }

    Result transact(const Bytes &frame);

    struct State {
        tkgm012::Settings settings;
        std::optional<tkgm012::DeviceInfo> info;
        std::optional<std::string> firmware;
    };
    // The vendor launch sweep: 0x10, 0x11, 0x12, 0x13, 0x14, 0x17, 0x20.
    // Registers that do not answer keep the values already in `state`.
    bool readAll(State &state);
    std::optional<tkgm012::DeviceInfo> readInfo();

    enum Register : unsigned {
        RegRate = 1,
        RegDpi = 2,
        RegColors = 4,
        RegPower = 8,
        RegKeys = 16,
        RegAll = 31,
    };
    // Writes the selected registers. Stops at the first rejection.
    Result write(const tkgm012::Settings &s, unsigned registers);
    Result factoryReset();

private:
    hid::Device m_dev;
    size_t m_outputSize = tkgm012::FrameSize; // report id + output report bytes
    std::string m_error;
};

} // namespace cheshub
