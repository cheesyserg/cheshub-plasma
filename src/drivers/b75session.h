// SPDX-License-Identifier: GPL-3.0-or-later
// Blocking iLovbee B75 session over hidraw: wired feature reports
// (CDevG5KB::AccessData) or the 2.4 GHz receiver's 0x13 reports
// (CDev3632::AccessData). Not thread-safe; the Qt layer runs it on a worker.
#pragma once

#include "../hid/hidraw.h"
#include "../protocol/b75.h"
#include "tkgm012session.h" // Result

#include <optional>
#include <string>

namespace cheshub {

class B75Session
{
public:
    enum class Link { Wired, Wireless, Bluetooth };

    // Wired board, receiver, or the (read-only) Bluetooth channel.
    static bool matches(const hid::NodeInfo &node);
    static std::optional<Link> linkFor(const hid::NodeInfo &node);

    bool open(const hid::NodeInfo &node);
    void close() { m_dev.close(); }
    Link link() const { return m_link; }
    const std::string &error() const { return m_error; }

    // Wired command numbers; translated for the receiver.
    std::optional<Bytes> read(uint8_t cmd, uint8_t sub, size_t length);
    bool write(uint8_t cmd, uint8_t sub, const Bytes &data);

    // Retries until the 5A A5 signature is present (vendor: five tries).
    std::optional<Bytes> readConfig();
    std::optional<b75::Power> readPower();
    // Receiver only: false when the keyboard is asleep or not linked.
    bool keyboardOnline();
    std::optional<Bytes> pairingId();

private:
    std::optional<Bytes> wiredRead(uint8_t cmd, uint8_t sub, size_t length);
    bool wiredWrite(uint8_t cmd, uint8_t sub, const Bytes &data);
    std::optional<Bytes> wirelessRead(uint8_t cmd, size_t length);
    bool wirelessWrite(uint8_t cmd, uint8_t sub, const Bytes &data);

    hid::Device m_dev;
    Link m_link = Link::Wired;
    uint8_t m_reportId = 0;
    size_t m_featureBytes = b75::FeaturePayload;
    std::string m_error;
};

} // namespace cheshub
