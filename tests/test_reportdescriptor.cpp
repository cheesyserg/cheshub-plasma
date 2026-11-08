// SPDX-License-Identifier: GPL-3.0-or-later
#include "hid/reportdescriptor.h"
#include "drivers/b75session.h"
#include "drivers/tkgm012session.h"

#include <QTest>

using namespace cheshub;
using namespace cheshub::hid;

class TestReportDescriptor : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void vendorMouseCollection()
    {
        // Usage Page 0xFF01, Usage 0x10, Collection(Application),
        // 32 x 8-bit input, 32 x 8-bit output, End Collection.
        const std::vector<uint8_t> d = {
            0x06, 0x01, 0xFF, 0x09, 0x10, 0xA1, 0x01,
            0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x20,
            0x09, 0x01, 0x81, 0x02,
            0x09, 0x02, 0x91, 0x02,
            0xC0};
        const auto cols = parseReportDescriptor(d);
        QCOMPARE(cols.size(), size_t(1));
        QCOMPARE(cols[0].usagePage, uint16_t(0xFF01));
        QCOMPARE(cols[0].usage, uint16_t(0x10));
        QCOMPARE(cols[0].find(Report::Kind::Input)->bytes(), 32u);
        QCOMPARE(cols[0].find(Report::Kind::Output)->bytes(), 32u);

        NodeInfo node;
        node.vendorId = tkgm012::WirelessVendorId;
        node.productId = tkgm012::WirelessProductId;
        node.collections = cols;
        QVERIFY(TkGm012Session::matches(node));
        node.collections[0].usagePage = 0x000C;
        QVERIFY(!TkGm012Session::matches(node));
    }

    void keyboardFeatureAndWireless()
    {
        // Two top-level collections: a mouse (ignored) and a vendor page
        // 0xFF00/1 with feature report 6 of 0x207 bytes.
        const std::vector<uint8_t> d = {
            0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0xC0, 0xC0,
            0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x06, 0x75, 0x08, 0x96, 0x07, 0x02,
            0x09, 0x01, 0xB1, 0x02, 0xC0};
        const auto cols = parseReportDescriptor(d);
        QCOMPARE(cols.size(), size_t(2));
        QCOMPARE(cols[0].usagePage, uint16_t(0x01));
        QCOMPARE(cols[0].find(Report::Kind::Input)->bits, 8u);
        const Report *f = cols[1].find(Report::Kind::Feature);
        QVERIFY(f);
        QCOMPARE(int(f->id), 6);
        QCOMPARE(f->bytes(), 0x207u);

        NodeInfo node;
        node.vendorId = b75::VendorId;
        node.productId = b75::WiredProductId;
        node.collections = cols;
        QCOMPARE(B75Session::linkFor(node).value(), B75Session::Link::Wired);

        // Receiver: 0xFF02 / 2 with 19-byte report 0x13 both ways.
        const std::vector<uint8_t> w = {
            0x06, 0x02, 0xFF, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x13, 0x75, 0x08, 0x95, 0x13,
            0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0xC0};
        node.productId = b75::ReceiverProductId;
        node.collections = parseReportDescriptor(w);
        QCOMPARE(node.collections[0].find(Report::Kind::Output, 0x13)->bytes(), 19u);
        QCOMPARE(B75Session::linkFor(node).value(), B75Session::Link::Wireless);

        node.vendorId = b75::BluetoothVendorId;
        node.productId = b75::BluetoothProductId;
        node.collections = {{0xFF00, 0x02, {{Report::Kind::Input, 3, 8}}}};
        QCOMPARE(B75Session::linkFor(node).value(), B75Session::Link::Bluetooth);
    }

    void truncatedIsSafe()
    {
        const std::vector<uint8_t> d = {0x06, 0x01};
        QVERIFY(parseReportDescriptor(d).empty());
    }
};

QTEST_GUILESS_MAIN(TestReportDescriptor)
#include "test_reportdescriptor.moc"
