// SPDX-License-Identifier: GPL-3.0-or-later
// Minimal HID report descriptor parser: top-level collections and the size of
// each input / output / feature report inside them. That is all that is needed
// to pick the vendor collection the way the web drivers filter on WebHID's
// `collections`.
#pragma once

#include <cstdint>
#include <vector>

namespace cheshub::hid {

struct Report {
    enum class Kind { Input, Output, Feature };
    Kind kind;
    uint8_t id; // 0 = unnumbered
    unsigned bits; // payload size, excluding the report id byte
    unsigned bytes() const { return (bits + 7) / 8; }
};

struct Collection {
    uint16_t usagePage = 0;
    uint16_t usage = 0;
    std::vector<Report> reports;

    const Report *find(Report::Kind kind, int id = -1) const;
    // Largest report of a kind, or nullptr.
    const Report *largest(Report::Kind kind) const;
};

std::vector<Collection> parseReportDescriptor(const uint8_t *data, size_t length);
inline std::vector<Collection> parseReportDescriptor(const std::vector<uint8_t> &d)
{
    return parseReportDescriptor(d.data(), d.size());
}

} // namespace cheshub::hid
