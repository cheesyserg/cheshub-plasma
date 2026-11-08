// SPDX-License-Identifier: GPL-3.0-or-later
// Small byte helpers shared by the protocol encoders. No Qt dependency, so the
// protocol layer can be unit-tested and reused from a CLI or a daemon.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cheshub {

using Bytes = std::vector<uint8_t>;

// 0xRRGGBB
using Rgb = uint32_t;

inline uint8_t red(Rgb c) { return (c >> 16) & 0xFF; }
inline uint8_t green(Rgb c) { return (c >> 8) & 0xFF; }
inline uint8_t blue(Rgb c) { return c & 0xFF; }
inline Rgb makeRgb(uint8_t r, uint8_t g, uint8_t b) { return (Rgb(r) << 16) | (Rgb(g) << 8) | Rgb(b); }

inline std::string toHex(const uint8_t *data, size_t len)
{
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 3);
    for (size_t i = 0; i < len; ++i) {
        if (i)
            out.push_back(' ');
        out.push_back(digits[data[i] >> 4]);
        out.push_back(digits[data[i] & 0x0F]);
    }
    return out;
}

inline std::string toHex(const Bytes &bytes, size_t limit = SIZE_MAX)
{
    return toHex(bytes.data(), bytes.size() < limit ? bytes.size() : limit);
}

// "00 08 00 01" / "00080001" -> bytes. Non-hex characters are ignored.
inline Bytes fromHex(const std::string &text)
{
    Bytes out;
    int nibble = -1;
    for (char ch : text) {
        int v;
        if (ch >= '0' && ch <= '9')
            v = ch - '0';
        else if (ch >= 'a' && ch <= 'f')
            v = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F')
            v = ch - 'A' + 10;
        else
            continue;
        if (nibble < 0) {
            nibble = v;
        } else {
            out.push_back(uint8_t((nibble << 4) | v));
            nibble = -1;
        }
    }
    return out;
}

} // namespace cheshub
