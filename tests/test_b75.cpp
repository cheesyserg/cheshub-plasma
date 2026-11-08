// SPDX-License-Identifier: GPL-3.0-or-later
// Checks against cheshub/devices/ilovbee-b75/docs/PROTOCOL.md and keymap.js.
#include "protocol/b75.h"

#include <QTest>

using namespace cheshub;
using namespace cheshub::b75;

static QByteArray hex(const uint8_t *p, size_t n)
{
    return QByteArray::fromStdString(toHex(p, n));
}

class TestB75 : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void wiredFrameLayout()
    {
        const Bytes data(600, 0xAB);
        const auto frames = b75::wiredWrite(0x06, SetMatrix, 0, data);
        QCOMPARE(frames.size(), size_t(2));
        QCOMPARE(frames[0].size(), FeatureReportBytes);
        QCOMPARE(hex(frames[0].data(), 9), QByteArray("06 03 00 00 02 00 00 02 ab"));
        QCOMPARE(hex(frames[1].data(), 9), QByteArray("06 03 00 00 02 01 58 00 ab"));
        QCOMPARE(int(frames[1][8 + 88]), 0); // only 88 bytes in the second packet

        const Bytes req = wiredReadRequest(0x06, GetConfig, 0, 1, 0, 0x80);
        QCOMPARE(hex(req.data(), 9), QByteArray("06 84 00 00 01 00 80 00 00"));
    }

    void wiredReplyOffsets()
    {
        Bytes withId(FeatureReportBytes, 0);
        withId[0] = 0x06;
        withId[1] = GetPower;
        withId[8] = 77;
        withId[9] = 1;
        QCOMPARE(int(wiredReplyData(withId, GetPower, 2).value()[0]), 77);
        Bytes withoutId(withId.begin() + 1, withId.end());
        QCOMPARE(int(wiredReplyData(withoutId, GetPower, 2).value()[1]), 1);
        QVERIFY(!wiredReplyData(withId, GetConfig, 2));
        auto p = parsePower(*wiredReplyData(withId, GetPower, 2));
        QCOMPARE(p->level.value(), 77);
        QVERIFY(p->charging);
    }

    void wirelessFrameChecksum()
    {
        const uint8_t data[3] = {1, 2, 3};
        const Bytes f = wirelessFrame(0x04, 10, 2, 0, data, 3);
        QCOMPARE(f.size(), size_t(20));
        QCOMPARE(hex(f.data(), 8), QByteArray("13 04 0a 02 03 01 02 03"));
        unsigned sum = 0;
        for (int i = 0; i < 19; ++i)
            sum += f[i];
        QCOMPARE(int(f[19]), int(sum & 0xFF));
        QCOMPARE(int(wirelessFrame(0x01, 1, 0, 2, nullptr, 0)[4]), 0x20); // sub << 4
        QCOMPARE(wirelessWrite(0x04, 0, Bytes(ConfigLength, 0)).size(), size_t(10));
        QCOMPARE(wirelessCommand(GetConfig).value(), uint8_t(0x44));
        QCOMPARE(wirelessCommand(SetRgbTable).value(), uint8_t(0x09));
    }

    void wirelessReader()
    {
        Bytes block(ConfigLength);
        for (size_t i = 0; i < block.size(); ++i)
            block[i] = uint8_t(i);
        // Reply packets in the same layout as writes.
        const auto packets = wirelessWrite(0x44, 0, block);
        WirelessReader reader(0x44, ConfigLength);
        QCOMPARE(reader.feed(wirelessFrame(0x07, 1, 0, 0, nullptr, 0)), WirelessReader::Result::Ignored);
        for (size_t i = 0; i < packets.size(); ++i) {
            const auto r = reader.feed(packets[i]);
            QCOMPARE(r, i + 1 == packets.size() ? WirelessReader::Result::Done : WirelessReader::Result::Pending);
        }
        QCOMPARE(reader.data(), block);

        Bytes rejected = packets[0];
        rejected[1] |= 0x80;
        WirelessReader other(0x44, ConfigLength);
        QCOMPARE(other.feed(rejected), WirelessReader::Result::Error);

        Bytes ack = wirelessFrame(0x04, 1, 0, 0, nullptr, 0);
        QCOMPARE(wirelessAck(ack, 0x04), WirelessAck::Ok);
        ack[2] |= 0x80;
        QCOMPARE(wirelessAck(ack, 0x04), WirelessAck::Resend);
        QCOMPARE(wirelessAck(ack, 0x09), WirelessAck::Ignored);
    }

    void configRoundTrip()
    {
        Bytes block(ConfigLength, 0);
        block[0x10] = 0x42; // unknown byte, must survive
        Settings s;
        s.effect = 2;
        s.effects[2] = {3, 1, 4, false};
        s.debounce = 1;
        s.sleep = 3;
        const Bytes out = encodeConfig(block, s);
        QVERIFY(hasSignature(out));
        QCOMPARE(int(out[0x10]), 0x42);
        QCOMPARE(int(out[cfg::DebounceA]), 0x04);
        QCOMPARE(int(out[cfg::DebounceB]), 0x04);
        QCOMPARE(int(out[cfg::Sleep]), 60);
        QCOMPARE(int(out[cfg::Mode]), 2);
        QCOMPARE(int(out[cfg::Color]), 4);
        QCOMPARE(int(out[cfg::Bright]), 15);
        QCOMPARE(int(out[cfg::Speed]), 1);
        QCOMPARE(int(out[cfg::ModeTable + 4]), 15);
        QCOMPARE(int(out[cfg::ModeTable + 5]), 0x14);
        QCOMPARE(int(out[cfg::ResetFlag]), 0);
        QCOMPARE(int(encodeConfig(block, s, true)[cfg::ResetFlag]), 1);

        Settings back;
        decodeConfig(out, back);
        QCOMPARE(back.effect, 2);
        QCOMPARE(back.effects[2], s.effects[2]);
        QCOMPARE(back.effects[5], s.effects[5]);
        QCOMPARE(back.debounce, 1);
        QCOMPARE(back.sleep, 3);
    }

    void decodeHotkeyChange()
    {
        Settings s;
        s.effect = 4;
        const Bytes before = encodeConfig(Bytes(ConfigLength, 0), s);
        Bytes after = before;
        after[cfg::Bright] = 5; // Fn+down moved only the active copy
        Settings read;
        decodeConfig(after, read, &before);
        QCOMPARE(read.effects[4].brightness, 1);
        // An untouched block keeps the table value.
        Settings again;
        decodeConfig(before, again, &before);
        QCOMPARE(again.effects[4].brightness, DefaultBrightness);
        QCOMPARE(brightnessIndex(100), 4);
        QCOMPARE(brightnessIndex(12), 2);
    }

    void neverWrittenTable()
    {
        Bytes block(ConfigLength, 0);
        block[cfg::SigA] = 0x5A;
        block[cfg::SigB] = 0xA5;
        Settings s;
        decodeConfig(block, s);
        QCOMPARE(s.effect, 0); // Off
        QCOMPARE(s.effects[3].brightness, DefaultBrightness); // zeros mean unset
    }

    void rgbTable()
    {
        Settings s;
        s.palettes[3][1] = 0x123456;
        const Bytes t = buildRgbTable(s);
        QCOMPARE(t.size(), RgbTableLength);
        QCOMPARE(hex(t.data() + 3 * 21 + 3, 3), QByteArray("12 34 56"));
        QCOMPARE(hex(t.data() + 21, 3), QByteArray("ff 00 00"));
        QCOMPARE(hex(t.data(), 3), QByteArray("00 00 00")); // id 0 (Off) unused
    }

    void factoryMatrix()
    {
        QCOMPARE(int(keys().size()), KeyCount);
        std::array<Assignment, KeyCount> none{};
        const Bytes m = buildMatrix(none, Layout::Knob);
        QCOMPARE(m.size(), MatrixLength * 4);
        QCOMPARE(hex(m.data() + 0 * 4, 4), QByteArray("00 00 00 29")); // Esc
        QCOMPARE(hex(m.data() + 4 * 4, 4), QByteArray("00 02 00 00")); // Left Shift
        QCOMPARE(hex(m.data() + 65 * 4, 4), QByteArray("0d 00 00 00")); // Fn
        QCOMPARE(hex(m.data() + 78 * 4, 4), QByteArray("02 00 00 e2")); // knob press
        QCOMPARE(hex(m.data() + 90 * 4, 4), QByteArray("00 00 00 4d")); // End stays factory
    }

    void remap()
    {
        std::array<Assignment, KeyCount> a{};
        const auto idx = [](int id) { return size_t(keyById(id) - keys().data()); };
        a[idx(1)] = {Assignment::Type::Keyboard, 0x01, 0x06, 0, 0}; // Esc -> Ctrl+C
        a[idx(45)] = {Assignment::Type::Keyboard, 0, 0xE0, 0, 0}; // Caps -> Left Ctrl
        a[idx(2)] = {Assignment::Type::Media, 0, 0, 0x0192, 0}; // F1 -> calculator
        a[idx(3)] = {Assignment::Type::Mouse, 0, 0, 0, 0x01010400}; // F2 -> back
        a[idx(4)] = {Assignment::Type::Disabled};
        a[idx(81)] = {Assignment::Type::Disabled}; // hidden on the knob layout
        const Bytes m = buildMatrix(a, Layout::Knob);
        QCOMPARE(hex(m.data() + 0, 4), QByteArray("00 01 00 06"));
        QCOMPARE(hex(m.data() + 3 * 4, 4), QByteArray("00 01 00 00"));
        QCOMPARE(hex(m.data() + 6 * 4, 4), QByteArray("02 00 01 92"));
        QCOMPARE(hex(m.data() + 12 * 4, 4), QByteArray("01 01 04 00"));
        QCOMPARE(hex(m.data() + 18 * 4, 4), QByteArray("00 00 00 00"));
        QCOMPARE(hex(m.data() + 90 * 4, 4), QByteArray("00 00 00 4d"));
        QCOMPARE(hex(buildMatrix(a, Layout::EndKey).data() + 90 * 4, 4), QByteArray("00 00 00 00"));
    }

    void keyRgbPlanes()
    {
        std::array<uint32_t, MatrixLength> colors;
        colors.fill(0xFF000000u);
        colors[5] = 0x102030;
        const Bytes p = buildKeyRgb(colors);
        QCOMPARE(p.size(), MatrixLength * 3);
        QCOMPARE(int(p[5]), 0x10);
        QCOMPARE(int(p[MatrixLength + 5]), 0x20);
        QCOMPARE(int(p[MatrixLength * 2 + 5]), 0x30);
        QCOMPARE(int(p[4]), 0);
    }

    void pairingId()
    {
        QVERIFY(isB75PairingId(fromHex("0a 00 00 00 00 12")));
        QVERIFY(!isB75PairingId(fromHex("0a 00 00 00 00 13")));
    }
};

QTEST_GUILESS_MAIN(TestB75)
#include "test_b75.moc"
