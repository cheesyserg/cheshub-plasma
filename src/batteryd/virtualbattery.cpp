// SPDX-License-Identifier: GPL-3.0-or-later
#include "virtualbattery.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/uhid.h>
#include <unistd.h>

#ifndef BUS_VIRTUAL
#define BUS_VIRTUAL 0x06
#endif

namespace cheshub::batteryd {

Bytes reportDescriptor(Kind kind)
{
    Bytes d;
    auto add = [&d](std::initializer_list<uint8_t> bytes) { d.insert(d.end(), bytes); };

    if (kind == Kind::Mouse) {
        add({0x05, 0x01, // Usage Page (Generic Desktop)
             0x09, 0x02, // Usage (Mouse)
             0xA1, 0x01, // Collection (Application)
             0x85, 0x01, //   Report ID (1)
             0x09, 0x01, //   Usage (Pointer)
             0xA1, 0x00, //   Collection (Physical)
             0x05, 0x09, //     Usage Page (Button)
             0x19, 0x01, //     Usage Minimum (1)
             0x29, 0x03, //     Usage Maximum (3)
             0x15, 0x00, //     Logical Minimum (0)
             0x25, 0x01, //     Logical Maximum (1)
             0x95, 0x03, //     Report Count (3)
             0x75, 0x01, //     Report Size (1)
             0x81, 0x02, //     Input (Data, Var, Abs)
             0x95, 0x01, //     Report Count (1)
             0x75, 0x05, //     Report Size (5)
             0x81, 0x01, //     Input (Const)
             0x05, 0x01, //     Usage Page (Generic Desktop)
             0x09, 0x30, //     Usage (X)
             0x09, 0x31, //     Usage (Y)
             0x15, 0x81, //     Logical Minimum (-127)
             0x25, 0x7F, //     Logical Maximum (127)
             0x75, 0x08, //     Report Size (8)
             0x95, 0x02, //     Report Count (2)
             0x81, 0x06, //     Input (Data, Var, Rel)
             0xC0}); //       End Collection
    } else {
        add({0x05, 0x01, // Usage Page (Generic Desktop)
             0x09, 0x06, // Usage (Keyboard)
             0xA1, 0x01, // Collection (Application)
             0x85, 0x01, //   Report ID (1)
             0x05, 0x07, //   Usage Page (Keyboard)
             0x19, 0xE0, //   Usage Minimum (Left Control)
             0x29, 0xE7, //   Usage Maximum (Right GUI)
             0x15, 0x00, //   Logical Minimum (0)
             0x25, 0x01, //   Logical Maximum (1)
             0x75, 0x01, //   Report Size (1)
             0x95, 0x08, //   Report Count (8)
             0x81, 0x02, //   Input (Data, Var, Abs)
             0x75, 0x08, //   Report Size (8)
             0x95, 0x06, //   Report Count (6)
             0x15, 0x00, //   Logical Minimum (0)
             0x25, 0x65, //   Logical Maximum (101)
             0x19, 0x00, //   Usage Minimum (0)
             0x29, 0x65, //   Usage Maximum (101)
             0x81, 0x00}); // Input (Data, Array)
    }
    // Battery, report 2, inside the same application collection.
    add({0x85, BatteryReportId, // Report ID (2)
         0x05, 0x06, //   Usage Page (Generic Device Controls)
         0x09, 0x20, //   Usage (Battery Strength)
         0x15, 0x00, //   Logical Minimum (0)
         0x25, 0x64, //   Logical Maximum (100)
         0x75, 0x08, //   Report Size (8)
         0x95, 0x01, //   Report Count (1)
         0x81, 0x02, //   Input (Data, Var, Abs)
         0x05, 0x85, //   Usage Page (Battery System)
         0x09, 0x44, //   Usage (Charging); used by kernels that know it
         0x25, 0x01, //   Logical Maximum (1)
         0x75, 0x01, //   Report Size (1)
         0x81, 0x02, //   Input (Data, Var, Abs)
         0x75, 0x07, //   Report Size (7)
         0x81, 0x01, //   Input (Const)
         0xC0}); //     End Collection
    return d;
}

Bytes batteryReport(int level, bool charging)
{
    const int clamped = level < 0 ? 0 : (level > 100 ? 100 : level);
    return {BatteryReportId, uint8_t(clamped), uint8_t(charging ? 1 : 0)};
}

namespace {
bool writeEvent(int fd, const uhid_event &ev)
{
    const ssize_t n = ::write(fd, &ev, sizeof ev);
    return n == ssize_t(sizeof ev);
}

void copyString(char *dst, size_t size, const std::string &src)
{
    std::strncpy(dst, src.c_str(), size - 1);
    dst[size - 1] = '\0';
}
} // namespace

VirtualBattery::~VirtualBattery()
{
    destroy();
}

bool VirtualBattery::create(Kind kind, const std::string &name, const std::string &uniq, uint16_t vendor, uint16_t product)
{
    destroy();
    m_fd = ::open("/dev/uhid", O_RDWR | O_CLOEXEC | O_NONBLOCK);
    if (m_fd < 0) {
        m_error = std::string("open /dev/uhid: ") + std::strerror(errno);
        return false;
    }
    const Bytes rd = reportDescriptor(kind);
    uhid_event ev{};
    ev.type = UHID_CREATE2;
    copyString(reinterpret_cast<char *>(ev.u.create2.name), sizeof ev.u.create2.name, name);
    copyString(reinterpret_cast<char *>(ev.u.create2.phys), sizeof ev.u.create2.phys, Phys);
    copyString(reinterpret_cast<char *>(ev.u.create2.uniq), sizeof ev.u.create2.uniq, uniq);
    ev.u.create2.rd_size = uint16_t(rd.size());
    // A virtual bus keeps the kernel's per-device quirks for the real USB
    // ids from applying, and keeps udev rules for the real device off it.
    ev.u.create2.bus = BUS_VIRTUAL;
    ev.u.create2.vendor = vendor;
    ev.u.create2.product = product;
    std::memcpy(ev.u.create2.rd_data, rd.data(), rd.size());
    if (!writeEvent(m_fd, ev)) {
        m_error = std::string("UHID_CREATE2: ") + std::strerror(errno);
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
    m_started = false;
    m_error.clear();
    return true;
}

void VirtualBattery::destroy()
{
    if (m_fd < 0)
        return;
    uhid_event ev{};
    ev.type = UHID_DESTROY;
    writeEvent(m_fd, ev);
    ::close(m_fd);
    m_fd = -1;
    m_started = false;
}

void VirtualBattery::setLevel(int level, bool charging)
{
    m_level = level;
    m_charging = charging;
    sendReport();
}

void VirtualBattery::sendReport()
{
    // Input reports sent before the kernel has bound a driver are dropped.
    if (m_fd < 0 || !m_started || m_level < 0)
        return;
    const Bytes report = batteryReport(m_level, m_charging);
    uhid_event ev{};
    ev.type = UHID_INPUT2;
    ev.u.input2.size = uint16_t(report.size());
    std::memcpy(ev.u.input2.data, report.data(), report.size());
    writeEvent(m_fd, ev);
}

void VirtualBattery::handleEvents()
{
    if (m_fd < 0)
        return;
    for (;;) {
        uhid_event ev{};
        const ssize_t n = ::read(m_fd, &ev, sizeof ev);
        if (n <= 0)
            return;
        switch (ev.type) {
        case UHID_START:
            m_started = true;
            sendReport();
            break;
        case UHID_STOP:
            m_started = false;
            break;
        case UHID_GET_REPORT: {
            // The kernel asks for the battery report until one has arrived.
            uhid_event reply{};
            reply.type = UHID_GET_REPORT_REPLY;
            reply.u.get_report_reply.id = ev.u.get_report.id;
            if (ev.u.get_report.rnum == BatteryReportId && m_level >= 0) {
                const Bytes report = batteryReport(m_level, m_charging);
                reply.u.get_report_reply.err = 0;
                reply.u.get_report_reply.size = uint16_t(report.size());
                std::memcpy(reply.u.get_report_reply.data, report.data(), report.size());
            } else {
                reply.u.get_report_reply.err = EIO;
            }
            writeEvent(m_fd, reply);
            break;
        }
        case UHID_SET_REPORT: {
            uhid_event reply{};
            reply.type = UHID_SET_REPORT_REPLY;
            reply.u.set_report_reply.id = ev.u.set_report.id;
            reply.u.set_report_reply.err = EIO;
            writeEvent(m_fd, reply);
            break;
        }
        default: // OPEN, CLOSE, OUTPUT: nothing to do
            break;
        }
    }
}

} // namespace cheshub::batteryd
