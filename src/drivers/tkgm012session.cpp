// SPDX-License-Identifier: GPL-3.0-or-later
#include "tkgm012session.h"

#include <chrono>

namespace cheshub {

using namespace tkgm012;

static constexpr int FirstWaitMs = 600;
static constexpr int RetryWaitMs = 1500;
static constexpr int PacketGapMs = 40; // driver.js applyAll spaces writes 40 ms apart

bool TkGm012Session::matches(const hid::NodeInfo &node)
{
    const bool ids = (node.vendorId == WiredVendorId && node.productId == WiredProductId)
        || (node.vendorId == WirelessVendorId && node.productId == WirelessProductId);
    return ids && node.hasCollection(VendorUsagePage, VendorUsage);
}

bool TkGm012Session::isWireless(const hid::NodeInfo &node)
{
    return node.vendorId == WirelessVendorId;
}

bool TkGm012Session::open(const hid::NodeInfo &node)
{
    if (!m_dev.open(node.devnode)) {
        m_error = m_dev.error();
        return false;
    }
    for (const hid::Collection &c : node.collections) {
        if (c.usagePage == VendorUsagePage && c.usage == VendorUsage) {
            if (const hid::Report *r = c.largest(hid::Report::Kind::Output))
                m_outputSize = 1 + r->bytes();
        }
    }
    return true;
}

static std::optional<Bytes> waitForAck(hid::Device &dev, uint8_t cmd, int timeoutMs, Bytes &last)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0)
            return std::nullopt;
        auto reply = dev.read(int(left));
        if (!reply)
            return std::nullopt;
        if (reply->size() > 1 && (*reply)[0] == cmd)
            last = *reply;
        if (isAck(*reply, cmd))
            return reply;
        // Stale acknowledgement of an earlier packet, or a rejection: keep
        // waiting, as DriverComm::SetDeviceData does.
    }
}

Result TkGm012Session::transact(const Bytes &frameIn)
{
    Result res;
    if (!m_dev.isOpen()) {
        res.error = "not connected";
        return res;
    }
    // hidraw wants exactly report id + report bytes; the frame is zero-padded
    // or trimmed like hidapi's _hid_write does.
    Bytes frame = frameIn;
    frame.resize(m_outputSize, 0);
    const uint8_t cmd = frame[1];

    hid::ExchangeLock lock(m_dev);
    m_dev.drain();
    Bytes last;
    for (int waitMs : {FirstWaitMs, RetryWaitMs}) {
        if (!m_dev.write(frame)) {
            res.error = m_dev.error();
            return res;
        }
        if (auto reply = waitForAck(m_dev, cmd, waitMs, last)) {
            res.ok = true;
            res.reply = *reply;
            return res;
        }
    }
    if (!last.empty()) {
        res.error = "device rejected command 0x" + toHex(&cmd, 1) + " (status " + std::to_string(last[1]) + ")";
        res.reply = last;
        return res;
    }
    res.ok = true;
    res.silent = true;
    return res;
}

std::optional<DeviceInfo> TkGm012Session::readInfo()
{
    Result r = transact(buildRead(Info));
    if (!r.ok || r.silent)
        return std::nullopt;
    return parseInfo(r.reply);
}

bool TkGm012Session::readAll(State &state)
{
    state.info = readInfo();
    if (!state.info) {
        m_error = "the mouse did not answer the info read (0x10) — is it switched on?";
        return false;
    }
    auto read = [&](uint8_t cmd, bool (*parse)(const Bytes &, Settings &)) {
        Result r = transact(buildRead(cmd));
        return r.ok && !r.silent && parse(r.reply, state.settings);
    };
    bool ok = true;
    ok &= read(KeysRead, parseKeys);
    ok &= read(RateRead, parseRate);
    ok &= read(DpiRead, parseDpi);
    ok &= read(DpiColorRead, parseDpiColors);
    ok &= read(PowerRead, parsePower);
    Result fw = transact(buildRead(Firmware));
    if (fw.ok && !fw.silent)
        state.firmware = parseFirmware(fw.reply);
    if (!ok)
        m_error = "some registers did not answer; their values were kept";
    return ok;
}

Result TkGm012Session::write(const Settings &s, unsigned registers)
{
    readInfo(); // wake the link the way the vendor does
    Result last;
    last.ok = true;
    bool anySilent = false;
    auto step = [&](unsigned reg, const Bytes &frame) {
        if (!(registers & reg) || !last.ok)
            return;
        last = transact(frame);
        anySilent |= last.silent;
        hid::sleepMs(PacketGapMs);
    };
    step(RegRate, rateFrame(s));
    step(RegDpi, dpiFrame(s));
    step(RegColors, dpiColorFrame(s));
    step(RegPower, powerFrame(s));
    step(RegKeys, keysFrame(s));
    if (last.ok)
        last.silent = anySilent;
    return last;
}

Result TkGm012Session::factoryReset()
{
    readInfo();
    return transact(factoryResetFrame());
}

} // namespace cheshub
