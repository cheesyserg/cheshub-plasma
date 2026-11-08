// SPDX-License-Identifier: GPL-3.0-or-later
// Linux hidraw transport. Equivalent of what WebHID does inside Chrome on Linux:
// the browser opens /dev/hidraw* directly, so the same udev rule grants access
// (see data/70-cheshub-plasma.rules).
#pragma once

#include "../protocol/bytes.h"
#include "reportdescriptor.h"

#include <optional>
#include <string>
#include <vector>

namespace cheshub::hid {

struct NodeInfo {
    std::string devnode; // /dev/hidrawN
    std::string sysname; // hidrawN
    uint16_t bus = 0; // 0x03 USB, 0x05 Bluetooth
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    std::string name; // HID_NAME
    std::string uniq; // HID_UNIQ (serial / BT address)
    std::vector<Collection> collections;

    bool hasCollection(uint16_t usagePage, int usage = -1) const;
};

// All hidraw nodes, read from /sys/class/hidraw. `sysRoot` exists for tests.
std::vector<NodeInfo> enumerate(const std::string &sysRoot = "/sys/class/hidraw");

class Device
{
public:
    Device() = default;
    ~Device();
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;

    // Returns false and sets error() on failure (EACCES -> udev hint).
    bool open(const std::string &devnode);
    void close();
    bool isOpen() const { return m_fd >= 0; }
    const std::string &error() const { return m_error; }

    // Output report. The first byte must be the report id (0 if unnumbered).
    bool write(const Bytes &report);
    // One input report, waiting up to timeoutMs. Unnumbered reports come back
    // without an id byte; numbered ones start with it.
    std::optional<Bytes> read(int timeoutMs);
    // Discards input reports that are already queued.
    void drain();

    // HIDIOCSFEATURE / HIDIOCGFEATURE. `report[0]` is the report id.
    bool setFeature(const Bytes &report);
    std::optional<Bytes> getFeature(uint8_t reportId, size_t length);

    // Exclusive advisory lock on the device node, held for one request/reply
    // exchange. cheshub-batteryd and System Settings both talk to the same
    // device; the lock keeps their exchanges from interleaving.
    void lock();
    void unlock();

private:
    void setErrno(const char *what);
    int m_fd = -1;
    std::string m_error;
};

// Holds Device::lock() for a scope.
class ExchangeLock
{
public:
    explicit ExchangeLock(Device &dev)
        : m_dev(dev)
    {
        m_dev.lock();
    }
    ~ExchangeLock() { m_dev.unlock(); }
    ExchangeLock(const ExchangeLock &) = delete;
    ExchangeLock &operator=(const ExchangeLock &) = delete;

private:
    Device &m_dev;
};

void sleepMs(int ms);

// What to check when opening a device is refused: the udev rule for a user,
// SELinux or AppArmor for root (which file permissions do not stop).
std::string accessDeniedHint();

} // namespace cheshub::hid
