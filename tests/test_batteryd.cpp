// SPDX-License-Identifier: GPL-3.0-or-later
// cheshub-batteryd's virtual device: descriptor shape and battery report.
#include "batteryd/virtualbattery.h"
#include "drivers/b75session.h"
#include "drivers/tkgm012session.h"
#include "hid/reportdescriptor.h"

#include <QTest>

using namespace cheshub;
using namespace cheshub::batteryd;
using hid::Report;

class TestBatteryd : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void mouseDescriptor()
    {
        const auto cols = hid::parseReportDescriptor(reportDescriptor(Kind::Mouse));
        QCOMPARE(cols.size(), size_t(1));
        QCOMPARE(cols[0].usagePage, uint16_t(0x01));
        QCOMPARE(cols[0].usage, uint16_t(0x02)); // mouse -> UPower shows a mouse
        QCOMPARE(cols[0].find(Report::Kind::Input, 1)->bits, 3u + 5u + 16u);
        QCOMPARE(cols[0].find(Report::Kind::Input, BatteryReportId)->bits, 16u);
        QVERIFY(!cols[0].find(Report::Kind::Output));
        QVERIFY(!cols[0].find(Report::Kind::Feature));
    }

    void keyboardDescriptor()
    {
        const auto cols = hid::parseReportDescriptor(reportDescriptor(Kind::Keyboard));
        QCOMPARE(cols.size(), size_t(1));
        QCOMPARE(cols[0].usage, uint16_t(0x06)); // keyboard
        QCOMPARE(cols[0].find(Report::Kind::Input, 1)->bytes(), 7u);
        QCOMPARE(cols[0].find(Report::Kind::Input, BatteryReportId)->bytes(), 2u);
    }

    void batteryUsagePresent()
    {
        // Generic Device Controls / Battery Strength, which hid-input turns
        // into a power_supply.
        for (Kind k : {Kind::Mouse, Kind::Keyboard}) {
            const Bytes d = reportDescriptor(k);
            const Bytes needle = {0x05, 0x06, 0x09, 0x20};
            QVERIFY(std::search(d.begin(), d.end(), needle.begin(), needle.end()) != d.end());
            QVERIFY(d.size() < 4096); // HID_MAX_DESCRIPTOR_SIZE
        }
    }

    void report()
    {
        QCOMPARE(batteryReport(70, false), (Bytes{2, 70, 0}));
        QCOMPARE(batteryReport(15, true), (Bytes{2, 15, 1}));
        QCOMPARE(batteryReport(150, false), (Bytes{2, 100, 0}));
        QCOMPARE(batteryReport(-3, false), (Bytes{2, 0, 0}));
    }

    void virtualDeviceIsNotMistakenForTheRealOne()
    {
        // The virtual battery gets its own hidraw node with the real ids;
        // neither the module nor the daemon may treat it as a device.
        hid::NodeInfo node;
        node.bus = 0x06;
        node.vendorId = tkgm012::WirelessVendorId;
        node.productId = tkgm012::WirelessProductId;
        node.collections = hid::parseReportDescriptor(reportDescriptor(Kind::Mouse));
        QVERIFY(!TkGm012Session::matches(node));

        node.vendorId = b75::VendorId;
        for (uint16_t pid : {b75::WiredProductId, b75::ReceiverProductId}) {
            node.productId = pid;
            node.collections = hid::parseReportDescriptor(reportDescriptor(Kind::Keyboard));
            QVERIFY(!B75Session::linkFor(node));
        }
    }
};

QTEST_GUILESS_MAIN(TestBatteryd)
#include "test_batteryd.moc"
