// SPDX-License-Identifier: GPL-3.0-or-later
#include "reportdescriptor.h"

#include <vector>

namespace cheshub::hid {

const Report *Collection::find(Report::Kind kind, int id) const
{
    for (const Report &r : reports) {
        if (r.kind == kind && (id < 0 || r.id == id))
            return &r;
    }
    return nullptr;
}

const Report *Collection::largest(Report::Kind kind) const
{
    const Report *best = nullptr;
    for (const Report &r : reports) {
        if (r.kind == kind && (!best || r.bits > best->bits))
            best = &r;
    }
    return best;
}

namespace {

struct Globals {
    uint16_t usagePage = 0;
    uint32_t reportSize = 0;
    uint32_t reportCount = 0;
    uint8_t reportId = 0;
};

void addBits(Collection &c, Report::Kind kind, uint8_t id, unsigned bits)
{
    for (Report &r : c.reports) {
        if (r.kind == kind && r.id == id) {
            r.bits += bits;
            return;
        }
    }
    c.reports.push_back({kind, id, bits});
}

} // namespace

std::vector<Collection> parseReportDescriptor(const uint8_t *data, size_t length)
{
    std::vector<Collection> out;
    Globals g;
    std::vector<Globals> stack;
    std::vector<uint32_t> usages;
    int depth = 0;
    Collection *current = nullptr;

    size_t i = 0;
    while (i < length) {
        const uint8_t prefix = data[i++];
        if (prefix == 0xFE) { // long item: [0xFE][size][tag][data...]
            if (i + 2 > length)
                break;
            i += 2 + data[i];
            continue;
        }
        size_t size = prefix & 0x03;
        if (size == 3)
            size = 4;
        if (i + size > length)
            break;
        uint32_t value = 0;
        for (size_t k = 0; k < size; ++k)
            value |= uint32_t(data[i + k]) << (8 * k);
        i += size;

        const int type = (prefix >> 2) & 0x03;
        const int tag = (prefix >> 4) & 0x0F;

        if (type == 0) { // main
            switch (tag) {
            case 0x8: // Input
            case 0x9: // Output
            case 0xB: // Feature
                if (current) {
                    const Report::Kind kind = tag == 0x8 ? Report::Kind::Input
                        : tag == 0x9                     ? Report::Kind::Output
                                                         : Report::Kind::Feature;
                    addBits(*current, kind, g.reportId, g.reportSize * g.reportCount);
                }
                break;
            case 0xA: // Collection
                if (depth == 0) {
                    Collection c;
                    const uint32_t u = usages.empty() ? 0 : usages.front();
                    c.usagePage = (u >> 16) ? uint16_t(u >> 16) : g.usagePage;
                    c.usage = uint16_t(u & 0xFFFF);
                    out.push_back(c);
                    current = &out.back();
                }
                ++depth;
                break;
            case 0xC: // End Collection
                if (depth > 0 && --depth == 0)
                    current = nullptr;
                break;
            default:
                break;
            }
            usages.clear();
        } else if (type == 1) { // global
            switch (tag) {
            case 0x0:
                g.usagePage = uint16_t(value);
                break;
            case 0x7:
                g.reportSize = value;
                break;
            case 0x8:
                g.reportId = uint8_t(value);
                break;
            case 0x9:
                g.reportCount = value;
                break;
            case 0xA:
                stack.push_back(g);
                break;
            case 0xB:
                if (!stack.empty()) {
                    g = stack.back();
                    stack.pop_back();
                }
                break;
            default:
                break;
            }
        } else if (type == 2) { // local
            if (tag == 0x0)
                usages.push_back(size == 4 ? value : (uint32_t(g.usagePage) << 16) | value);
        }
    }
    return out;
}

} // namespace cheshub::hid
