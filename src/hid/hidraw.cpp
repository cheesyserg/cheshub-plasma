// SPDX-License-Identifier: GPL-3.0-or-later
#include "hidraw.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <linux/hidraw.h>
#include <poll.h>
#include <sstream>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

namespace cheshub::hid {

bool NodeInfo::hasCollection(uint16_t usagePage, int usage) const
{
    for (const Collection &c : collections) {
        if (c.usagePage == usagePage && (usage < 0 || c.usage == usage))
            return true;
    }
    return false;
}

namespace {

std::string readText(const std::string &path)
{
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

Bytes readBinary(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// HID_ID=0003:0000248A:00005D2E
void parseUevent(const std::string &text, NodeInfo &info)
{
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.rfind("HID_ID=", 0) == 0) {
            unsigned bus = 0, vid = 0, pid = 0;
            if (std::sscanf(line.c_str() + 7, "%x:%x:%x", &bus, &vid, &pid) == 3) {
                info.bus = uint16_t(bus);
                info.vendorId = uint16_t(vid);
                info.productId = uint16_t(pid);
            }
        } else if (line.rfind("HID_NAME=", 0) == 0) {
            info.name = line.substr(9);
        } else if (line.rfind("HID_UNIQ=", 0) == 0) {
            info.uniq = line.substr(9);
        }
    }
}

} // namespace

std::vector<NodeInfo> enumerate(const std::string &sysRoot)
{
    std::vector<NodeInfo> out;
    DIR *dir = opendir(sysRoot.c_str());
    if (!dir)
        return out;
    while (dirent *ent = readdir(dir)) {
        if (std::strncmp(ent->d_name, "hidraw", 6) != 0)
            continue;
        NodeInfo info;
        info.sysname = ent->d_name;
        info.devnode = "/dev/" + info.sysname;
        const std::string base = sysRoot + "/" + info.sysname + "/device/";
        parseUevent(readText(base + "uevent"), info);
        info.collections = parseReportDescriptor(readBinary(base + "report_descriptor"));
        out.push_back(std::move(info));
    }
    closedir(dir);
    std::sort(out.begin(), out.end(), [](const NodeInfo &a, const NodeInfo &b) {
        return a.sysname.size() != b.sysname.size() ? a.sysname.size() < b.sysname.size() : a.sysname < b.sysname;
    });
    return out;
}

Device::~Device()
{
    close();
}

void Device::setErrno(const char *what)
{
    const int err = errno;
    m_error = std::string(what) + ": " + std::strerror(err);
    if (err == EACCES || err == EPERM)
        m_error += " — install the cheshub-plasma udev rule and re-plug the device";
    else if (err == EBUSY)
        m_error += " — another program is using the device";
}

bool Device::open(const std::string &devnode)
{
    close();
    m_fd = ::open(devnode.c_str(), O_RDWR | O_CLOEXEC | O_NONBLOCK);
    if (m_fd < 0) {
        setErrno(("open " + devnode).c_str());
        return false;
    }
    m_error.clear();
    return true;
}

void Device::close()
{
    if (m_fd >= 0)
        ::close(m_fd);
    m_fd = -1;
}

bool Device::write(const Bytes &report)
{
    if (m_fd < 0) {
        m_error = "not open";
        return false;
    }
    for (int attempt = 0; attempt < 3; ++attempt) {
        const ssize_t n = ::write(m_fd, report.data(), report.size());
        if (n == ssize_t(report.size()))
            return true;
        if (n < 0 && errno != EAGAIN && errno != EINTR) {
            setErrno("write");
            return false;
        }
        sleepMs(5);
    }
    m_error = "write: short or busy";
    return false;
}

std::optional<Bytes> Device::read(int timeoutMs)
{
    if (m_fd < 0)
        return std::nullopt;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        pollfd p{m_fd, POLLIN, 0};
        const int r = ::poll(&p, 1, int(std::max<long long>(0, left)));
        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0)
            return std::nullopt;
        if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            m_error = "device disconnected";
            return std::nullopt;
        }
        uint8_t buf[256];
        const ssize_t n = ::read(m_fd, buf, sizeof buf);
        if (n > 0)
            return Bytes(buf, buf + n);
        if (n < 0 && (errno == EAGAIN || errno == EINTR))
            continue;
        setErrno("read");
        return std::nullopt;
    }
}

void Device::drain()
{
    while (read(0)) { }
}

bool Device::setFeature(const Bytes &report)
{
    if (m_fd < 0) {
        m_error = "not open";
        return false;
    }
    Bytes buf = report;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (::ioctl(m_fd, HIDIOCSFEATURE(buf.size()), buf.data()) >= 0)
            return true;
        setErrno("set feature");
        sleepMs(50); // vendor retries three times with 50 ms pauses
    }
    return false;
}

std::optional<Bytes> Device::getFeature(uint8_t reportId, size_t length)
{
    if (m_fd < 0)
        return std::nullopt;
    Bytes buf(length, 0);
    buf[0] = reportId;
    const int n = ::ioctl(m_fd, HIDIOCGFEATURE(buf.size()), buf.data());
    if (n < 0) {
        setErrno("get feature");
        return std::nullopt;
    }
    buf.resize(size_t(n));
    return buf;
}

void sleepMs(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

} // namespace cheshub::hid
