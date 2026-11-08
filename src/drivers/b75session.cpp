// SPDX-License-Identifier: GPL-3.0-or-later
#include "b75session.h"

#include <chrono>

namespace cheshub {

using namespace b75;

namespace {

// CDevG5KB::FindHIDDevice: vendor collection with a 0x208-byte feature report.
const hid::Report *pickFeature(const hid::NodeInfo &node)
{
    const hid::Report *best = nullptr;
    int bestScore = -1;
    for (const hid::Collection &c : node.collections) {
        for (const hid::Report &r : c.reports) {
            if (r.kind != hid::Report::Kind::Feature)
                continue;
            const int score = (c.usagePage >= 0xFF00 ? 2 : 0) + (r.bytes() == FeaturePayload ? 4 : 0) + (r.bytes() >= 64 ? 1 : 0);
            if (score > bestScore) {
                bestScore = score;
                best = &r;
            }
        }
    }
    // Anything below "vendor page and at least 64 bytes" is not the config channel.
    return bestScore >= 3 ? best : nullptr;
}

bool hasWirelessChannel(const hid::NodeInfo &node)
{
    for (const hid::Collection &c : node.collections) {
        if (c.usagePage == 0xFF02 && c.usage == 0x02 && c.find(hid::Report::Kind::Output, WirelessReportId))
            return true;
    }
    return false;
}

long long msUntil(std::chrono::steady_clock::time_point deadline)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
}

} // namespace

std::optional<B75Session::Link> B75Session::linkFor(const hid::NodeInfo &node)
{
    if (node.vendorId == BluetoothVendorId && node.productId == BluetoothProductId && node.hasCollection(0xFF00, 0x02))
        return Link::Bluetooth;
    if (node.vendorId != VendorId)
        return std::nullopt;
    if (node.productId == ReceiverProductId && hasWirelessChannel(node))
        return Link::Wireless;
    if (node.productId == WiredProductId && pickFeature(node))
        return Link::Wired;
    return std::nullopt;
}

bool B75Session::matches(const hid::NodeInfo &node)
{
    return linkFor(node).has_value();
}

bool B75Session::open(const hid::NodeInfo &node)
{
    auto link = linkFor(node);
    if (!link) {
        m_error = "not a B75 configuration channel";
        return false;
    }
    m_link = *link;
    if (m_link == Link::Bluetooth) {
        m_error = "Bluetooth is read-only on this keyboard: its vendor channel only sends a status report "
                  "and cannot receive settings. Use the USB cable or the 2.4 GHz receiver.";
        return false;
    }
    if (m_link == Link::Wired) {
        const hid::Report *r = pickFeature(node);
        m_reportId = r->id;
        m_featureBytes = r->bytes();
    }
    if (!m_dev.open(node.devnode)) {
        m_error = m_dev.error();
        return false;
    }
    return true;
}

std::optional<Bytes> B75Session::read(uint8_t cmd, uint8_t sub, size_t length)
{
    hid::ExchangeLock lock(m_dev);
    if (m_link == Link::Wireless) {
        auto wl = wirelessCommand(cmd);
        return wirelessRead(wl ? *wl : cmd, length);
    }
    return wiredRead(cmd, sub, length);
}

bool B75Session::write(uint8_t cmd, uint8_t sub, const Bytes &data)
{
    hid::ExchangeLock lock(m_dev);
    if (m_link == Link::Wireless) {
        auto wl = wirelessCommand(cmd);
        if (!wl) {
            m_error = "not available over the receiver";
            return false;
        }
        return wirelessWrite(*wl, sub, data);
    }
    return wiredWrite(cmd, sub, data);
}

// ------------------------------------------------------------------ wired

bool B75Session::wiredWrite(uint8_t cmd, uint8_t sub, const Bytes &data)
{
    for (const Bytes &frame : b75::wiredWrite(m_reportId, cmd, sub, data, m_featureBytes)) {
        hid::sleepMs(WiredPacketGapMs);
        if (!m_dev.setFeature(frame)) {
            m_error = m_dev.error();
            return false;
        }
    }
    return true;
}

std::optional<Bytes> B75Session::wiredRead(uint8_t cmd, uint8_t sub, size_t length)
{
    Bytes out;
    const size_t packets = std::max<size_t>(1, (length + WiredChunk - 1) / WiredChunk);
    for (size_t i = 0; i < packets; ++i) {
        const size_t size = std::min(WiredChunk, length - out.size());
        hid::sleepMs(WiredPacketGapMs);
        if (!m_dev.setFeature(wiredReadRequest(m_reportId, cmd, sub, uint8_t(packets), uint8_t(i), uint16_t(size), m_featureBytes))) {
            m_error = m_dev.error();
            return std::nullopt;
        }
        // The vendor waits 25 ms before fetching the reply; asking earlier can
        // return the previous answer still in the buffer.
        std::optional<Bytes> data;
        for (int wait : {WiredPacketGapMs, 15, 25}) {
            hid::sleepMs(wait);
            if (auto reply = m_dev.getFeature(m_reportId, 1 + m_featureBytes)) {
                data = wiredReplyData(*reply, cmd, size);
                if (data)
                    break;
            }
        }
        if (!data) {
            m_error = "no reply to command 0x" + toHex(&cmd, 1);
            return std::nullopt;
        }
        out.insert(out.end(), data->begin(), data->end());
    }
    return out;
}

// --------------------------------------------------------------- wireless

bool B75Session::wirelessWrite(uint8_t cmd, uint8_t sub, const Bytes &data)
{
    for (const Bytes &frame : b75::wirelessWrite(cmd, sub, data)) {
        for (int attempt = 0;; ++attempt) {
            hid::sleepMs(WirelessGapMs);
            m_dev.drain();
            if (!m_dev.write(frame)) {
                m_error = m_dev.error();
                return false;
            }
            WirelessAck ack = WirelessAck::Ignored;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(WirelessTimeoutMs);
            while (ack == WirelessAck::Ignored && msUntil(deadline) > 0) {
                auto report = m_dev.read(int(msUntil(deadline)));
                if (!report)
                    break;
                ack = wirelessAck(*report, cmd);
            }
            if (ack == WirelessAck::Ok)
                break;
            if (ack == WirelessAck::Error) {
                m_error = "the receiver reported an error";
                return false;
            }
            if (ack == WirelessAck::Ignored) {
                m_error = "the keyboard did not answer — is it awake?";
                return false;
            }
            if (attempt >= 5) {
                m_error = "the keyboard kept asking for a resend";
                return false;
            }
        }
    }
    return true;
}

std::optional<Bytes> B75Session::wirelessRead(uint8_t cmd, size_t length)
{
    hid::sleepMs(WirelessGapMs);
    m_dev.drain();
    if (!m_dev.write(wirelessReadRequest(cmd))) {
        m_error = m_dev.error();
        return std::nullopt;
    }
    WirelessReader reader(cmd, length);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (msUntil(deadline) > 0) {
        auto report = m_dev.read(int(msUntil(deadline)));
        if (!report)
            break;
        switch (reader.feed(*report)) {
        case WirelessReader::Result::Done:
            return reader.data();
        case WirelessReader::Result::Error:
            m_error = "the keyboard rejected the request";
            return std::nullopt;
        default:
            break;
        }
    }
    m_error = "the keyboard did not answer — is it awake?";
    return std::nullopt;
}

// ------------------------------------------------------------------ misc

std::optional<Bytes> B75Session::readConfig()
{
    for (int attempt = 0; attempt < 5; ++attempt) {
        auto block = read(GetConfig, 0, ConfigLength);
        if (block && hasSignature(*block))
            return block;
        hid::sleepMs(100);
    }
    if (m_error.empty())
        m_error = "config block signature (5A A5) missing";
    return std::nullopt;
}

std::optional<Power> B75Session::readPower()
{
    auto data = read(GetPower, 0, 2);
    return data ? parsePower(*data) : std::nullopt;
}

bool B75Session::keyboardOnline()
{
    if (m_link != Link::Wireless)
        return true;
    auto st = read(WirelessOnline, 0, 1);
    // Older receivers may not answer; carry on as the web driver does.
    return !st || (*st)[0] != 0;
}

std::optional<Bytes> B75Session::pairingId()
{
    return read(GetPairingId, 1, 6);
}

} // namespace cheshub
