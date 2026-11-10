#include "discovery_test_peer.h"
#include "robot_discovery.h"

#include <QRandomGenerator>
#include <QtTest>

namespace {
QByteArray goldenQuery() {
  return QByteArray::fromHex(
      "1234000000010000000000000d726f626f72696f2d312d667263056c6f63616c0000010001");
}
QByteArray goldenResponse() {
  return QByteArray::fromHex(
      "1234840000010001000000000d726f626f72696f2d312d667263056c6f63616c0000010001"
      "c00c000100010000003c00040a000102");
}
} // namespace

class DiscoveryTest : public QObject {
  Q_OBJECT
private slots:
  void queryAndCompressedAnswerHaveIndependentWireVectors() {
    QString error;
    QCOMPARE(ds::discovery::query(1, 0x1234, error).value(), goldenQuery());
    const auto records = ds::discovery::parseResponse(goldenResponse(), 1, 0x1234, error);
    QVERIFY2(records, qPrintable(error));
    QCOMPARE(records->size(), 1);
    QCOMPARE(records->first().address, QHostAddress("10.0.1.2"));
    QCOMPARE(records->first().ttlSeconds, quint32(60));
    auto uppercaseQuestion = goldenQuery();
    uppercaseQuestion.replace("roborio", "ROBORIO").replace("frc", "FRC").replace("local", "LOCAL");
    QVERIFY(ds::discovery::parseResponse(ds::test::dnsReply(uppercaseQuestion, {0x0a000102}), 1,
                                         0x1234, error));
  }

  void teamHintsRespectOctetLimitsAndAvoidLeadingZeros() {
    QCOMPARE(ds::discovery::robotHostname(1), QStringLiteral("roborio-1-frc.local"));
    QCOMPARE(ds::discovery::robotHostname(65535), QStringLiteral("roborio-65535-frc.local"));
    for (const int team : {-1, 0, 65536}) {
      QVERIFY(ds::discovery::robotHostname(team).isEmpty());
      QString error;
      QVERIFY(!ds::discovery::query(team, 1, error));
    }
    QCOMPARE(ds::discovery::teamAddress(10327).value(), QHostAddress("10.103.27.2"));
    QCOMPARE(ds::discovery::teamAddress(25599).value(), QHostAddress("10.255.99.2"));
    QVERIFY(!ds::discovery::teamAddress(25600));
    QVERIFY(!ds::discovery::teamAddress(65535));
    QVERIFY(!ds::discovery::teamAddress(0));
  }

  void labelBoundariesCannotAliasTheRequestedHostname() {
    // One label containing a literal dot is a different DNS name from two labels.
    const auto singleLabel = QByteArray::fromHex("13726f626f72696f2d312d6672632e6c6f63616c00");
    QString error;
    auto wrongQuestion = goldenResponse();
    wrongQuestion.replace(12, 21, singleLabel);
    QVERIFY(!ds::discovery::parseResponse(wrongQuestion, 1, 0x1234, error));

    auto wrongOwner = goldenResponse();
    wrongOwner.replace(goldenQuery().size(), 2, singleLabel);
    const auto records = ds::discovery::parseResponse(wrongOwner, 1, 0x1234, error);
    QVERIFY2(records, qPrintable(error));
    QVERIFY(records->isEmpty());
  }

  void expandedNameLengthIncludesLabelAndRootBytes_data() {
    QTest::addColumn<int>("lastLabelLength");
    QTest::addColumn<bool>("valid");
    QTest::newRow("255-wire-bytes") << 61 << true;
    QTest::newRow("256-wire-bytes") << 62 << false;
  }
  void expandedNameLengthIncludesLabelAndRootBytes() {
    QFETCH(int, lastLabelLength);
    QFETCH(bool, valid);
    QByteArray owner;
    for (const auto length : {63, 63, 63, lastLabelLength}) {
      owner.append(char(length));
      owner.append(QByteArray(length, 'x'));
    }
    owner.append('\0');
    auto packet = goldenResponse();
    packet.replace(goldenQuery().size(), 2, owner);
    QString error;
    const auto records = ds::discovery::parseResponse(packet, 1, 0x1234, error);
    QCOMPARE(records.has_value(), valid);
    if (records)
      QVERIFY(records->isEmpty());
  }

  void malformedDnsIsRejected_data() {
    QTest::addColumn<int>("kind");
    for (int kind = 0; kind < 11; ++kind)
      QTest::newRow(qPrintable(QString::number(kind))) << kind;
  }
  void malformedDnsIsRejected() {
    QFETCH(int, kind);
    auto packet = goldenResponse();
    const int owner = goldenQuery().size();
    if (kind == 0)
      packet[1] = char(0x35); // Different transaction ID.
    if (kind == 1)
      packet[2] = 0; // Query, not response.
    if (kind == 2)
      packet[2] = char(0x86); // Truncated.
    if (kind == 3)
      packet[3] = 3; // Negative result.
    if (kind == 4)
      packet[5] = 0; // Missing repeated question.
    if (kind == 5)
      packet[7] = 65; // Record count cap.
    if (kind == 6)
      packet[owner + 1] = char(owner); // Self-referential compression.
    if (kind == 7) {
      packet[12] = char(0xc0);
      packet[13] = 12;
    } // Question loop.
    if (kind == 8)
      packet[packet.size() - 5] = 3; // Invalid A RDATA length.
    if (kind == 9)
      packet.append('x'); // Trailing bytes.
    if (kind == 10)
      packet[12] = char(0x40); // Reserved DNS label format.
    QString error;
    QVERIFY(!ds::discovery::parseResponse(packet, 1, 0x1234, error));
    QVERIFY(!error.isEmpty());
  }

  void unrelatedAddressesAndIpv6DoNotBecomeCandidates() {
    QString error;
    auto packet = ds::test::dnsReply(goldenQuery(), {0, 0xffffffff, 0xe00000fb, 0x0a000102});
    const auto result = ds::discovery::parseResponse(packet, 1, 0x1234, error);
    QVERIFY(result);
    QCOMPARE(result->size(), 1);
    packet = goldenResponse();
    packet[goldenQuery().size() + 3] =
        28; // Unknown/AAAA record treated as opaque, not an A address.
    QVERIFY(ds::discovery::parseResponse(packet, 1, 0x1234, error)->isEmpty());
    packet = goldenResponse();
    packet.replace(goldenQuery().size(), 2, QByteArray::fromHex("06776964676574056c6f63616c00"));
    QVERIFY(ds::discovery::parseResponse(packet, 1, 0x1234, error)->isEmpty());
    QVERIFY(!ds::discovery::usableIpv4(QHostAddress::LocalHostIPv6));
  }

  void everyTruncationAndMutationIsBounded() {
    const auto packet = goldenResponse();
    QString error;
    for (qsizetype length = 0; length < packet.size(); ++length)
      QVERIFY(!ds::discovery::parseResponse(packet.left(length), 1, 0x1234, error));
    QRandomGenerator random(5353);
    for (int iteration = 0; iteration < 10000; ++iteration) {
      auto mutated = packet;
      mutated[int(random.bounded(quint32(mutated.size())))] = char(random.generate());
      const auto records = ds::discovery::parseResponse(mutated, 1, 0x1234, error);
      if (records)
        QVERIFY(records->size() <= ds::discovery::MaxRecords);
    }
    QVERIFY(!ds::discovery::parseResponse(QByteArray(ds::discovery::MaxDatagram + 1, 'x'), 1,
                                          0x1234, error));
  }

  void compressedPointerChainsAreSupportedWithinTheirHopLimit() {
    QString error;
    const auto chained = [](int count) {
      const auto query = goldenQuery();
      auto packet = ds::test::dnsReply(query, QVector<quint32>(count, 0x0a000102));
      for (int record = 1; record < count; ++record) {
        const int offset = query.size() + record * 16;
        const int target = offset - 16;
        packet[offset] = char(0xc0 | (target >> 8));
        packet[offset + 1] = char(target & 0xff);
      }
      return packet;
    };
    const auto records = ds::discovery::parseResponse(chained(16), 1, 0x1234, error);
    QVERIFY2(records, qPrintable(error));
    QCOMPARE(records->size(), 16);
    QVERIFY(!ds::discovery::parseResponse(chained(17), 1, 0x1234, error));
  }

  void realUdpDiscoveryCachesOnlyBoundedFreshAnswers() {
    ds::test::DiscoveryPeer peer;
    QVERIFY(peer.socket.localPort());
    for (int i = 1; i <= 30; ++i)
      peer.addresses.append(0x0a000100U + quint32(i));
    ds::RobotDiscovery discovery(nullptr, {QHostAddress::LocalHost, peer.socket.localPort()});
    QCOMPARE(discovery.sentCount(), quint64(0));
    QVERIFY(discovery.start(10327));
    QTRY_COMPARE(discovery.addresses().size(), ds::discovery::MaxAddresses);
    QVERIFY(discovery.active());
    QCOMPARE(discovery.sentCount(), quint64(1));
    peer.send(ds::test::dnsReply(peer.last.data(), {0x7f000001}, 0));
    QTRY_VERIFY(!discovery.addresses().contains(QHostAddress::LocalHost));
    discovery.cancel();
    QVERIFY(!discovery.active());
    QVERIFY(discovery.addresses().isEmpty());
    const auto count = peer.queries.size();
    QTest::qWait(1100);
    QCOMPARE(peer.queries.size(), count); // No delayed retry after cancellation.
  }

  void wrongSourceIdAndOldSearchCannotAddResults() {
    ds::test::DiscoveryPeer peer;
    peer.replying = false;
    ds::RobotDiscovery discovery(nullptr, {QHostAddress::LocalHost, peer.socket.localPort()});
    QVERIFY(discovery.start(1));
    QTRY_VERIFY(!peer.queries.isEmpty());
    const auto old = ds::test::dnsReply(peer.last.data(), {0x7f000001});
    QUdpSocket stranger;
    QVERIFY(stranger.bind(QHostAddress::LocalHost, 0));
    stranger.writeDatagram(old, peer.last.senderAddress(), peer.last.senderPort());
    QTRY_VERIFY(discovery.rejectedCount() > 0);
    QVERIFY(discovery.addresses().isEmpty());
    const auto count = peer.queries.size();
    QVERIFY(discovery.start(2));
    QTRY_VERIFY(peer.queries.size() > count);
    peer.send(old);
    QTRY_VERIFY(discovery.rejectedCount() > 0);
    QVERIFY(discovery.addresses().isEmpty());
    peer.send(ds::test::dnsReply(peer.last.data(), {0x7f000001}));
    QTRY_COMPARE(discovery.addresses().size(), 1);
  }

  void timeoutMakesOnlyTwoQueriesAndTtlExpires() {
    ds::test::DiscoveryPeer peer;
    peer.replying = false;
    ds::RobotDiscovery discovery(nullptr, {QHostAddress::LocalHost, peer.socket.localPort()});
    QVERIFY(discovery.start(1));
    QTRY_VERIFY_WITH_TIMEOUT(!discovery.active(), 4000);
    QCOMPARE(peer.queries.size(), 2);
    QVERIFY(discovery.addresses().isEmpty());
    peer.replying = true;
    peer.ttl = 1;
    QVERIFY(discovery.start(1));
    QTRY_COMPARE(discovery.addresses().size(), 1);
    peer.replying = false;
    QTRY_VERIFY_WITH_TIMEOUT(discovery.addresses().isEmpty(), 1800);
  }

  void invalidTargetsAndTeamsNeverSendQueries() {
    ds::RobotDiscovery invalid(nullptr, {QHostAddress("192.0.2.1"), 5353});
    QVERIFY(!invalid.start(1));
    QCOMPARE(invalid.sentCount(), quint64(0));
    ds::RobotDiscovery unset(nullptr, {QHostAddress::LocalHost, 12345});
    QVERIFY(!unset.start(0));
    QCOMPARE(unset.sentCount(), quint64(0));
  }

  void delayedEventLoopCannotSendALateRetryOrAcceptALateAnswer() {
    ds::test::DiscoveryPeer peer;
    peer.replying = false;
    ds::RobotDiscovery discovery(nullptr, {QHostAddress::LocalHost, peer.socket.localPort()});
    QVERIFY(discovery.start(1));
    QTRY_COMPARE(peer.queries.size(), 1);
    QTest::qSleep(ds::RobotDiscovery::SearchTimeoutMs + 50);
    peer.send(ds::test::dnsReply(peer.last.data(), {0x7f000001}));
    QTRY_VERIFY(!discovery.active());
    QVERIFY(discovery.addresses().isEmpty());
    QCOMPARE(peer.queries.size(), 1);
  }
};

QTEST_GUILESS_MAIN(DiscoveryTest)
#include "test_discovery.moc"
