// SPDX-License-Identifier: GPL-3.0-or-later
// Byte-exact checks against the captures in
// cheshub/devices/tecknet-tk-gm012/docs/{PROTOCOL,REGISTERS}.md.
#include "protocol/tkgm012.h"

#include <QTest>

using namespace cheshub;
using namespace cheshub::tkgm012;

static QByteArray hex(const Bytes &b)
{
    return QByteArray::fromStdString(toHex(b));
}

class TestTkGm012 : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void dpiCodec()
    {
        QCOMPARE(encodeDpi(1100), uint16_t(0x16));
        QCOMPARE(encodeDpi(24000), uint16_t(0xE8));
        QCOMPARE(encodeDpi(50), uint16_t(0x01));
        QCOMPARE(decodeDpi(0xCC), 10400);
        QCOMPARE(decodeDpi(0xDC), 12000);
        QCOMPARE(decodeDpi(0xE8), 24000);
        for (int dpi : {50, 800, 10000, 10100, 12000, 13000, 24000})
            QCOMPARE(decodeDpi(encodeDpi(dpi)), dpi);
        QCOMPARE(snapDpi(1234), 1250);
        QCOMPARE(snapDpi(11149), 11100);
        // Ambiguous vendor range: 12500 encodes to 225, which decodes as 17000.
        QCOMPARE(decodeDpi(encodeDpi(12500)), 17000);
        QCOMPARE(snapDpi(12400), 12000);
        QCOMPARE(snapDpi(12600), 13000);
        QCOMPARE(snapDpi(13600), 14000);
        QCOMPARE(snapDpi(99999), 24000);
    }

    void rateFrame()
    {
        Settings s;
        s.rateHz = 250;
        const Bytes f = tkgm012::rateFrame(s);
        QCOMPARE(f.size(), size_t(33));
        QCOMPARE(hex(Bytes(f.begin(), f.begin() + 6)), QByteArray("00 02 00 01 01 04"));
        QCOMPARE(f[32], uint8_t(0x04));
        QCOMPARE(decodeRate(0x08).value(), 125);
        QVERIFY(!decodeRate(0x03));
    }

    void dpiFrame()
    {
        Settings s;
        s.stageCount = 6;
        s.activeStage = 2;
        s.dpi = {1100, 24000, 2400, 3200, 6400, 12000};
        const Bytes f = tkgm012::dpiFrame(s);
        // 03 00 01 25 | P | 16 00 16 00 | e8 00 e8 00 | ...
        QCOMPARE(hex(Bytes(f.begin() + 1, f.begin() + 14)), QByteArray("03 00 01 25 26 16 00 16 00 e8 00 e8 00"));
        QCOMPARE(f[32], checksum(f));
    }

    void powerFrame()
    {
        // Captured: wake on, 6 min -> 07 00 01 04 24 01 01 00
        Settings s;
        s.sleepMinutes = 6;
        s.moveWake = true;
        const Bytes f = tkgm012::powerFrame(s);
        QCOMPARE(hex(Bytes(f.begin() + 1, f.begin() + 9)), QByteArray("07 00 01 04 24 01 01 00"));
        s.moveWake = false;
        const Bytes off = tkgm012::powerFrame(s);
        QCOMPARE(hex(Bytes(off.begin() + 1, off.begin() + 9)), QByteArray("07 00 01 04 24 00 01 00"));
    }

    void colorAndKeyFrames()
    {
        Settings s;
        const Bytes c = dpiColorFrame(s);
        QCOMPARE(hex(Bytes(c.begin() + 1, c.begin() + 8)), QByteArray("04 00 01 12 ff 00 00"));
        const Bytes k = keysFrame(s);
        QCOMPARE(hex(Bytes(k.begin() + 1, k.begin() + 23)),
                 QByteArray("09 00 01 0f 10 01 00 10 02 00 10 04 00 10 08 00 10 10 00 40 01 00"));
    }

    void resetFrames()
    {
        const Bytes f = factoryResetFrame();
        QCOMPARE(hex(Bytes(f.begin(), f.begin() + 7)), QByteArray("00 0f 00 01 01 01 ff"));
        QCOMPARE(f[32], uint8_t(0x00)); // (0x01 + 0xFF) & 0xFF
        const Bytes r = resetIndicatorFrame();
        QCOMPARE(hex(Bytes(r.begin(), r.begin() + 6)), QByteArray("00 0e 00 01 01 01"));
    }

    void parseDpiCapture()
    {
        // 0x13 reply captured on launch (packed byte 00 = keep count).
        Bytes reply = fromHex("13 00 01 19 00 16 00 16 00 cc 00 cc 00 30 00 30 00 40 00 40 00 80 00 80 00 dc 00 dc 00 00 00 00");
        Settings s;
        QVERIFY(parseDpi(reply, s));
        QCOMPARE(s.dpi[0], 1100);
        QCOMPARE(s.dpi[1], 10400);
        QCOMPARE(s.dpi[2], 2400);
        QCOMPARE(s.dpi[3], 3200);
        QCOMPARE(s.dpi[4], 6400);
        QCOMPARE(s.dpi[5], 12000);
        QCOMPARE(s.stageCount, 6);
        QCOMPARE(s.activeStage, 0);
        reply[4] = 0x34; // stage 4 active, 4 stages
        QVERIFY(parseDpi(reply, s));
        QCOMPARE(s.stageCount, 4);
        QCOMPARE(s.activeStage, 3);
    }

    void parsePowerCapture()
    {
        Settings s;
        s.moveWake = true;
        QVERIFY(parsePower(fromHex("17 00 01 05 1e 00 01 00 00"), s));
        QCOMPARE(s.sleepMinutes, 5);
        QCOMPARE(s.moveWake, false);
        QCOMPARE(int(s.powerAux), 1);
    }

    void parseKeysDefaults()
    {
        Settings s;
        s.keys[0] = {0x60, 0, 0};
        QVERIFY(parseKeys(fromHex("11 00 01 12 10 01 00 10 02 00 10 04 00 10 08 00 10 10 00 40 01 00 00 00"), s));
        QCOMPARE(s, Settings{});
        QCOMPARE(std::string(actionForRecord(s.keys[3])->id), std::string("back"));
        QCOMPARE(recordClassName({0x90, 0x25, 0x01}), std::string("Macro"));
    }

    void parseInfoAndFirmware()
    {
        auto info = parseInfo(fromHex("10 00 01 0b 4c 39 33 51 00 00 11 01 00 46 01 00 00 00"));
        QVERIFY(info);
        QCOMPARE(info->deviceId, std::string("L93Q"));
        QCOMPARE(info->battery.value(), 70);
        QCOMPARE(info->charging, false);
        QCOMPARE(info->linkState, 1);
        QCOMPARE(parseFirmware(fromHex("20 00 01 02 26 03")).value(), std::string("3.26"));
    }

    void ack()
    {
        QVERIFY(isAck(fromHex("02 00 00 01"), 0x02));
        QVERIFY(!isAck(fromHex("02 01 00 01"), 0x02));
        QVERIFY(!isAck(fromHex("03 00 00 01"), 0x02));
    }

    void tables()
    {
        QCOMPARE(int(buttons().size()), ButtonCount);
        QCOMPARE(buttons()[3].keyValue, 4); // front side button is key_value 4
        QVERIFY(actionById("dpi-loop"));
        QCOMPARE(nearestSleep(7), 6);
    }
};

QTEST_GUILESS_MAIN(TestTkGm012)
#include "test_tkgm012.moc"
