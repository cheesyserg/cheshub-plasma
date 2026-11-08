// SPDX-License-Identifier: GPL-3.0-or-later
// cheshub-batteryd — puts the battery level of supported wireless mice and
// keyboards into UPower, so Plasma's Power & Battery applet shows them.
//
// Every few seconds it looks for supported devices among /dev/hidraw*. For
// each one it reads the battery over the vendor protocol (once a minute,
// opening the device only for that read) and mirrors it on a virtual HID
// battery (see virtualbattery.h). Each read is one locked exchange, so it
// never interleaves with System Settings talking to the same device.
//
// Runs as root (systemd unit cheshub-batteryd.service): /dev/uhid is
// root-only.
#include "../drivers/b75session.h"
#include "../drivers/tkgm012session.h"
#include "virtualbattery.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <poll.h>
#include <string>
#include <vector>

using namespace cheshub;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

namespace {

constexpr auto ScanInterval = 5s;
constexpr auto PollInterval = 60s;
constexpr auto RetryInterval = 15s; // until the first reading
constexpr auto StaleAfter = 15min; // receiver present, device switched off

volatile std::sig_atomic_t g_stop = 0;

void onSignal(int)
{
    g_stop = 1;
}

// stderr goes to the journal under systemd.
void log(const std::string &message)
{
    std::fprintf(stderr, "%s\n", message.c_str());
    std::fflush(stderr);
}

struct Reading {
    int level;
    bool charging;
};

struct Tracked {
    hid::NodeInfo node;
    batteryd::Kind kind;
    std::string name;
    batteryd::VirtualBattery battery;
    Clock::time_point nextPoll = Clock::now();
    Clock::time_point lastGood = Clock::now();
    bool everRead = false;
    std::string lastWhy = "-"; // forces the first log line
};

// One battery read. On failure `why` says what happened, with the raw
// bytes the device sent, so the journal shows why nothing was published.
struct Attempt {
    std::optional<Reading> reading;
    std::string why;
};

Attempt readBattery(const Tracked &t)
{
    if (t.kind == batteryd::Kind::Mouse) {
        TkGm012Session s;
        if (!s.open(t.node))
            return {std::nullopt, "cannot open " + t.node.devnode + ": " + s.error()};
        const Result r = s.transact(tkgm012::buildRead(tkgm012::Info));
        if (!r.ok)
            return {std::nullopt, "info read (0x10) failed: " + r.error};
        if (r.silent)
            return {std::nullopt, "no answer to the info read (0x10); mouse asleep or switched off?"};
        auto info = tkgm012::parseInfo(r.reply);
        if (!info || !info->battery)
            return {std::nullopt, "info reply has no battery level: " + toHex(r.reply, 16)};
        return {Reading{*info->battery, info->charging}, {}};
    }
    B75Session s;
    if (!s.open(t.node))
        return {std::nullopt, "cannot open " + t.node.devnode + ": " + s.error()};
    if (!s.keyboardOnline())
        return {std::nullopt, "keyboard asleep or not linked to its receiver"};
    auto raw = s.read(b75::GetPower, 0, 2);
    if (!raw)
        return {std::nullopt, "power read (0x87) failed: " + s.error()};
    auto power = b75::parsePower(*raw);
    if (!power || !power->level)
        return {std::nullopt, "power reply has no battery level: " + toHex(*raw)};
    return {Reading{*power->level, power->charging}, {}};
}

std::optional<std::pair<batteryd::Kind, std::string>> classify(const hid::NodeInfo &node)
{
    if (TkGm012Session::matches(node))
        return std::make_pair(batteryd::Kind::Mouse, std::string("TECKNET TK-GM012"));
    auto link = B75Session::linkFor(node);
    // Over Bluetooth the board's own BLE battery service already reaches
    // UPower through BlueZ.
    if (link && *link != B75Session::Link::Bluetooth)
        return std::make_pair(batteryd::Kind::Keyboard, std::string("iLovbee B75"));
    return std::nullopt;
}

void scan(std::map<std::string, std::unique_ptr<Tracked>> &tracked)
{
    std::map<std::string, bool> present;
    for (const hid::NodeInfo &node : hid::enumerate()) {
        auto kind = classify(node);
        if (!kind)
            continue;
        present[node.devnode] = true;
        if (tracked.count(node.devnode))
            continue;
        auto t = std::make_unique<Tracked>();
        t->node = node;
        t->kind = kind->first;
        t->name = kind->second;
        log("found " + t->name + " on " + node.devnode);
        tracked[node.devnode] = std::move(t);
    }
    for (auto it = tracked.begin(); it != tracked.end();) {
        if (!present.count(it->first)) {
            log(it->second->name + " on " + it->first + " removed");
            it = tracked.erase(it); // destroys the virtual battery
        } else {
            ++it;
        }
    }
}

void pollDevice(Tracked &t)
{
    const auto now = Clock::now();
    const Attempt attempt = readBattery(t);
    const auto &reading = attempt.reading;
    // Log each new reason once, and the first reading after a failure.
    if (attempt.why != t.lastWhy) {
        log(t.name + ": " + (reading ? "battery " + std::to_string(reading->level) + "%" : attempt.why));
        t.lastWhy = attempt.why;
    }
    if (reading) {
        t.lastGood = now;
        t.everRead = true;
        if (!t.battery.isCreated()) {
            // hid-<uniq>-battery; the hidraw name keeps two identical
            // devices apart.
            const std::string uniq = "cheshub-" + t.node.sysname;
            if (t.battery.create(t.kind, t.name, uniq, t.node.vendorId, t.node.productId))
                log(t.name + ": battery published as hid-" + uniq + "-battery");
            else
                log(t.name + ": " + t.battery.error());
        }
        t.battery.setLevel(reading->level, reading->charging);
        t.nextPoll = now + PollInterval;
        return;
    }
    if (t.battery.isCreated() && now - t.lastGood > StaleAfter) {
        log(t.name + ": no reading for 15 minutes, battery withdrawn");
        t.battery.destroy();
    }
    t.nextPoll = now + (t.everRead ? PollInterval : RetryInterval);
}

} // namespace

int main()
{
    std::signal(SIGTERM, onSignal);
    std::signal(SIGINT, onSignal);
    std::signal(SIGPIPE, SIG_IGN);

    std::map<std::string, std::unique_ptr<Tracked>> tracked;
    auto nextScan = Clock::now();

    while (!g_stop) {
        auto now = Clock::now();
        if (now >= nextScan) {
            scan(tracked);
            nextScan = now + ScanInterval;
        }
        for (auto &[devnode, t] : tracked) {
            if (Clock::now() >= t->nextPoll)
                pollDevice(*t);
        }

        // Sleep until the next scan or poll, answering the kernel meanwhile.
        auto wake = nextScan;
        std::vector<pollfd> fds;
        std::vector<Tracked *> owners;
        for (auto &[devnode, t] : tracked) {
            wake = std::min(wake, t->nextPoll);
            if (t->battery.isCreated()) {
                fds.push_back({t->battery.fd(), POLLIN, 0});
                owners.push_back(t.get());
            }
        }
        const auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(wake - Clock::now()).count();
        const int n = ::poll(fds.data(), fds.size(), int(std::max<long long>(0, std::min<long long>(wait, 5000))));
        if (n > 0) {
            for (size_t i = 0; i < fds.size(); ++i) {
                if (fds[i].revents & POLLIN)
                    owners[i]->battery.handleEvents();
            }
        }
    }
    tracked.clear(); // UHID_DESTROY for every battery
    return 0;
}
