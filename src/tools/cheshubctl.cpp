// SPDX-License-Identifier: GPL-3.0-or-later
// cheshubctl — check access to supported devices and dump what they report,
// without System Settings. Read-only on purpose.
//
//   cheshubctl list    supported hidraw nodes and whether they can be opened
//   cheshubctl read    read every supported device's settings
//   cheshubctl nodes   every hidraw node with its top-level collections
#include "drivers/b75session.h"
#include "drivers/tkgm012session.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace cheshub;

static const char *kindOf(const hid::NodeInfo &n)
{
    if (TkGm012Session::matches(n))
        return TkGm012Session::isWireless(n) ? "TECKNET TK-GM012 (2.4 GHz receiver)" : "TECKNET TK-GM012 (USB)";
    if (auto link = B75Session::linkFor(n)) {
        switch (*link) {
        case B75Session::Link::Wired:
            return "iLovbee B75 (USB)";
        case B75Session::Link::Wireless:
            return "iLovbee B75 (2.4 GHz receiver)";
        case B75Session::Link::Bluetooth:
            return "iLovbee B75 (Bluetooth, read-only)";
        }
    }
    return nullptr;
}

static int cmdNodes()
{
    for (const hid::NodeInfo &n : hid::enumerate()) {
        std::printf("%s  %04x:%04x bus %02x  %s\n", n.devnode.c_str(), n.vendorId, n.productId, n.bus, n.name.c_str());
        for (const hid::Collection &c : n.collections) {
            std::printf("    usage page 0x%04x usage 0x%04x:", c.usagePage, c.usage);
            for (const hid::Report &r : c.reports) {
                const char *k = r.kind == hid::Report::Kind::Input ? "in" : r.kind == hid::Report::Kind::Output ? "out" : "feature";
                std::printf(" %s#%u=%uB", k, r.id, r.bytes());
            }
            std::printf("\n");
        }
    }
    return 0;
}

static int cmdList()
{
    int found = 0;
    for (const hid::NodeInfo &n : hid::enumerate()) {
        const char *kind = kindOf(n);
        if (!kind)
            continue;
        ++found;
        hid::Device d;
        const bool ok = d.open(n.devnode);
        std::printf("%-14s %s — %s\n", n.devnode.c_str(), kind, ok ? "accessible" : d.error().c_str());
    }
    if (!found)
        std::printf("No supported device found.\n");
    return found ? 0 : 1;
}

static void readMouse(const hid::NodeInfo &n)
{
    TkGm012Session s;
    if (!s.open(n)) {
        std::printf("  error: %s\n", s.error().c_str());
        return;
    }
    const Result raw = s.transact(tkgm012::buildRead(tkgm012::Info));
    std::printf("  [info reply %s]\n", raw.ok && !raw.silent ? toHex(raw.reply, 16).c_str()
                                                             : (raw.silent ? "no answer" : raw.error.c_str()));
    TkGm012Session::State st;
    const bool ok = s.readAll(st);
    if (!st.info) {
        std::printf("  error: %s\n", s.error().c_str());
        return;
    }
    const tkgm012::Settings &c = st.settings;
    std::printf("  device id %s, battery %s%s, firmware %s\n", st.info->deviceId.c_str(),
                st.info->battery ? (std::to_string(*st.info->battery) + "%").c_str() : "n/a",
                st.info->charging ? " (charging)" : "", st.firmware ? st.firmware->c_str() : "n/a");
    std::printf("  report rate %d Hz\n", c.rateHz);
    std::printf("  DPI stages (%d in use, stage %d active):", c.stageCount, c.activeStage + 1);
    for (int i = 0; i < tkgm012::StageCount; ++i)
        std::printf(" %d/#%06x", c.dpi[i], c.colors[i]);
    std::printf("\n  sleep %d min, move wake %s\n", c.sleepMinutes, c.moveWake ? "on" : "off");
    for (const tkgm012::Button &b : tkgm012::buttons()) {
        const auto &rec = c.keys[b.keyValue];
        const tkgm012::Action *a = tkgm012::actionForRecord(rec);
        std::printf("  %-22s %s [%s]\n", b.label, a ? a->label : tkgm012::recordClassName(rec).c_str(),
                    toHex(rec.data(), rec.size()).c_str());
    }
    if (!ok)
        std::printf("  warning: %s\n", s.error().c_str());
}

static void readKeyboard(const hid::NodeInfo &n)
{
    B75Session s;
    if (!s.open(n)) {
        std::printf("  error: %s\n", s.error().c_str());
        return;
    }
    if (!s.keyboardOnline()) {
        std::printf("  the keyboard is asleep — press a key and try again\n");
        return;
    }
    auto block = s.readConfig();
    if (!block) {
        std::printf("  error: %s\n", s.error().c_str());
        return;
    }
    b75::Settings st;
    b75::decodeConfig(*block, st);
    const b75::Effect *e = b75::effectByHw(st.effect);
    const b75::EffectState &cur = st.effects[st.effect];
    std::printf("  effect %s, brightness %d/4, speed %d/4, colour %s\n", e ? e->name : "?", cur.brightness, cur.speed,
                cur.colorful ? "colourful" : std::to_string(cur.slot + 1).c_str());
    std::printf("  debounce %s, sleep %s\n", b75::debounceOptions()[st.debounce].name, b75::sleepOptions()[st.sleep].name);
    if (auto raw = s.read(b75::GetPower, 0, 2)) {
        auto p = b75::parsePower(*raw);
        std::printf("  battery %s%s  [power reply %s]\n", p && p->level ? (std::to_string(*p->level) + "%").c_str() : "n/a",
                    p && p->charging ? " (charging)" : "", toHex(*raw).c_str());
    } else {
        std::printf("  battery: power read failed: %s\n", s.error().c_str());
    }
    std::printf("  config block: %s\n", toHex(*block).c_str());
}

static int cmdRead()
{
    int found = 0;
    for (const hid::NodeInfo &n : hid::enumerate()) {
        const char *kind = kindOf(n);
        if (!kind)
            continue;
        ++found;
        std::printf("%s — %s\n", n.devnode.c_str(), kind);
        if (TkGm012Session::matches(n))
            readMouse(n);
        else
            readKeyboard(n);
    }
    if (!found)
        std::printf("No supported device found.\n");
    return found ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *cmd = argc > 1 ? argv[1] : "list";
    if (!std::strcmp(cmd, "list"))
        return cmdList();
    if (!std::strcmp(cmd, "read"))
        return cmdRead();
    if (!std::strcmp(cmd, "nodes"))
        return cmdNodes();
    std::fprintf(stderr, "usage: cheshubctl [list|read|nodes]\n");
    return 2;
}
