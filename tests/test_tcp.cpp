#include "tcp_robot_link.h"
#include "tcp_test_peer.h"

#include <QRandomGenerator>
#include <QtTest>

namespace {
QByteArray hex(const char* data) { return QByteArray::fromHex(data); }
ds::tcp::Frame frame(quint8 id, const char* data) { return {false, id, hex(data)}; }
} // namespace

class TcpTest : public QObject {
  Q_OBJECT
private slots:
  void descriptorsHaveExplicitBytesForAllSlotsAndUtf8Lengths() {
    QVector<ds::tcp::JoystickDescriptor> descriptors(6);
    descriptors[0] = {QStringLiteral("Pad"), {0xff, 0xff}, 10, 1};
    QString error;
    const auto encoded = ds::tcp::encodeDescriptors(descriptors, error);
    QVERIFY2(encoded, qPrintable(error));
    QByteArray expected = hex("000d020000ff0350616402ffff0a01");
    for (quint8 slot = 1; slot < 6; ++slot) {
      auto empty = hex("0008020000ff00000000");
      empty[3] = char(slot);
      expected.append(empty);
    }
    QCOMPARE(*encoded, expected);
    QCOMPARE(ds::tcp::encodeGameData(QStringLiteral("Aç"), error).value(), hex("00040e41c3a7"));
    QCOMPARE(ds::tcp::encodeGameData({}, error).value(), hex("00010e"));
    descriptors[0].name = QStringLiteral("ç");
    const auto unicode = ds::tcp::encodeDescriptors(descriptors, error);
    QVERIFY(unicode);
    QCOMPARE(static_cast<quint8>((*unicode)[6]), quint8(2));
  }

  void descriptorAndGameDataBoundsAreEnforced_data() {
    QTest::addColumn<int>("kind");
    for (int kind = 0; kind < 7; ++kind)
      QTest::newRow(qPrintable(QString::number(kind))) << kind;
  }
  void descriptorAndGameDataBoundsAreEnforced() {
    QFETCH(int, kind);
    QVector<ds::tcp::JoystickDescriptor> descriptors(6);
    if (kind == 0)
      descriptors.resize(5);
    if (kind == 1)
      descriptors[0].name = QString(128, QChar(u'ç'));
    if (kind == 2)
      descriptors[0].axisTypes.fill(0xff, 13);
    if (kind == 3)
      descriptors[0].buttons = 33;
    if (kind == 4)
      descriptors[0].povs = 13;
    if (kind == 5)
      descriptors[0].name = QString(QChar(u'\0'));
    QString error;
    if (kind == 6)
      QVERIFY(!ds::tcp::encodeGameData(QString(513, QChar(u'ç')), error));
    else
      QVERIFY(!ds::tcp::encodeDescriptors(descriptors, error));
    QVERIFY(!error.isEmpty());
  }

  void fragmentedAndCoalescedFramesIncludeHeartbeatsAndUnknownTags() {
    const auto bytes = hex("0000 000199 000a0c3f8000001234616263 00090a0000000001520131");
    for (qsizetype split = 0; split <= bytes.size(); ++split) {
      ds::tcp::StreamDecoder decoder;
      QString error;
      QVector<ds::tcp::Frame> frames;
      for (const auto& chunk : {bytes.left(split), bytes.mid(split)}) {
        QVERIFY(decoder.feed(chunk, error));
        while (const auto value = decoder.take(error))
          frames.append(*value);
        QVERIFY(error.isEmpty());
      }
      QCOMPARE(frames.size(), 4);
      QVERIFY(frames[0].heartbeat);
      QCOMPARE(ds::tcp::decodeMessage(frames[1], error)->kind, ds::tcp::Message::Kind::Ignored);
      const auto stdoutMessage = ds::tcp::decodeMessage(frames[2], error);
      QVERIFY2(stdoutMessage, qPrintable(error));
      QCOMPARE(stdoutMessage->seconds, 1.0f);
      QCOMPARE(stdoutMessage->sequence, quint16(0x1234));
      QCOMPARE(stdoutMessage->text, QStringLiteral("abc"));
      const auto version = ds::tcp::decodeMessage(frames[3], error);
      QVERIFY2(version, qPrintable(error));
      QCOMPARE(version->name, QStringLiteral("R"));
      QCOMPARE(version->version, QStringLiteral("1"));
      QCOMPARE(decoder.bufferedBytes(), 0);
    }
  }

  void errorAndVersionMessagesHaveIndependentGoldenPayloads() {
    QString error;
    const auto message = ds::tcp::decodeMessage(
        frame(0x0b, "40000000 ffff 0001 fffffffe 01 0003 626164 0002 666e 0000"), error);
    QVERIFY2(message, qPrintable(error));
    QCOMPARE(message->kind, ds::tcp::Message::Kind::Error);
    QCOMPARE(message->seconds, 2.0f);
    QCOMPARE(message->sequence, quint16(65535));
    QCOMPARE(message->errorCode, qint32(-2));
    QVERIFY(message->isError);
    QCOMPARE(message->text, QStringLiteral("bad"));
    QCOMPARE(message->location, QStringLiteral("fn"));
    QVERIFY(message->stack.isEmpty());
    const auto end = ds::tcp::decodeMessage(frame(0x0a, "000000000000"), error);
    QVERIFY(end && end->endOfVersions);
    const auto controls = ds::tcp::decodeMessage(frame(0x0c, "00000000 0000 1b3c623e0a"), error);
    QVERIFY(controls);
    QCOMPARE(controls->text, QString(QChar::ReplacementCharacter) + QStringLiteral("<b>\n"));
  }

  void malformedKnownMessagesAreRejected_data() {
    QTest::addColumn<int>("id");
    QTest::addColumn<QByteArray>("payload");
    QTest::newRow("stdout-short") << 0x0c << hex("0000000000");
    QTest::newRow("nan") << 0x0c << hex("7fc000000000");
    QTest::newRow("infinity") << 0x0c << hex("7f8000000000");
    QTest::newRow("negative-time") << 0x0c << hex("bf8000000000");
    QTest::newRow("utf8") << 0x0c << hex("000000000000c328");
    QTest::newRow("utf8-incomplete") << 0x0c << hex("000000000000c3");
    QTest::newRow("error-short") << 0x0b << hex("000000000000");
    QTest::newRow("error-length") << 0x0b << hex("00000000000000010000000001000400");
    QTest::newRow("error-flags") << 0x0b << hex("00000000000000010000000004000000000000");
    QTest::newRow("version-short") << 0x0a << hex("0000000000");
    QTest::newRow("version-length") << 0x0a << hex("00000000ff00");
    QTest::newRow("version-extra") << 0x0a << hex("00000000000000");
  }
  void malformedKnownMessagesAreRejected() {
    QFETCH(int, id);
    QFETCH(QByteArray, payload);
    QString error;
    QVERIFY(!ds::tcp::decodeMessage({false, static_cast<quint8>(id), payload}, error));
    QVERIFY(!error.isEmpty());
  }

  void streamBoundsAndDeterministicMalformedSweep() {
    ds::tcp::StreamDecoder decoder;
    QString error;
    QVERIFY(!decoder.feed(QByteArray(ds::tcp::MaxBuffer + 1, 'x'), error));
    QCOMPARE(decoder.bufferedBytes(), 0);
    QVERIFY(decoder.feed(hex("ffff"), error));
    QVERIFY(!decoder.take(error));
    QVERIFY(!error.isEmpty());
    const auto largest = ds::tcp::encodeFrame(0x99, QByteArray(ds::tcp::MaxFrame - 1, 'x'), error);
    QVERIFY(largest);
    QVERIFY(decoder.feed(*largest, error));
    QVERIFY(decoder.take(error));
    QVERIFY(!ds::tcp::encodeFrame(0x99, QByteArray(ds::tcp::MaxFrame, 'x'), error));
    QRandomGenerator random(1740);
    for (int iteration = 0; iteration < 10000; ++iteration) {
      QByteArray bytes(random.bounded(256), '\0');
      for (auto& value : bytes)
        value = char(random.generate() & 0xff);
      decoder.feed(bytes, error);
      int budget = 64;
      while (budget-- > 0) {
        const auto packet = decoder.take(error);
        if (!packet)
          break;
        ds::tcp::decodeMessage(*packet, error);
      }
      QVERIFY(decoder.bufferedBytes() <= ds::tcp::MaxBuffer);
    }
  }

  void realTcpSynchronizesReplaysAndReceivesFragmentedConsole() {
    ds::test::TcpPeer peer;
    QVERIFY(peer.server.isListening());
    ds::TcpRobotLink link;
    QVector<ds::tcp::JoystickDescriptor> descriptors(6);
    descriptors[2] = {QStringLiteral("Flight"), {0xff, 0xff}, 8, 1};
    QVERIFY(link.setStationData(descriptors, QStringLiteral("ABC")));
    QVERIFY(link.start(QHostAddress::LocalHost, peer.server.serverPort()));
    QTRY_VERIFY(link.ready());
    QTRY_COMPARE(peer.frames.size(), 7);
    QCOMPARE(peer.frames[2].id, quint8(2));
    QCOMPARE(peer.frames[2].payload, hex("0200ff06466c6967687402ffff0801"));
    QCOMPARE(peer.frames.last().payload, QByteArray("ABC"));
    const auto sent = link.sentCount();
    QVERIFY(link.setStationData(descriptors, QStringLiteral("ABC")));
    QCOMPARE(link.sentCount(), sent);
    QStringList logs;
    connect(&link, &ds::TcpRobotLink::messageReceived, this, [&](const auto& message) {
      if (message.kind == ds::tcp::Message::Kind::Stdout)
        logs.append(message.text);
    });
    peer.send(hex("00"));
    QTest::qWait(20);
    QCOMPARE(link.receivedCount(), quint64(0));
    peer.send(hex("0a0c3f8000000001616263 00090a0000000001520131"));
    QTRY_COMPARE(logs, QStringList({QStringLiteral("abc")}));
    QCOMPARE(link.versions().size(), 1);
    descriptors[2] = {};
    QVERIFY(link.setStationData(descriptors, QStringLiteral("DEF")));
    QTRY_VERIFY(link.ready());
    QTRY_COMPARE(peer.frames.size(), 14);
    QCOMPARE(peer.frames[9].payload, hex("0200ff00000000"));
    QCOMPARE(peer.frames.last().payload, QByteArray("DEF"));
    link.stop();
    QVERIFY(link.versions().isEmpty());
    QVERIFY(link.start(QHostAddress::LocalHost, peer.server.serverPort()));
    QTRY_VERIFY(link.ready());
    QTRY_COMPARE(peer.connections, 2);
    QTRY_COMPARE(peer.frames.size(), 21);
    QCOMPARE(peer.frames.last().payload, QByteArray("DEF"));
  }

  void malformedLengthPartialTimeoutAndFloodCloseTheStream_data() {
    QTest::addColumn<int>("kind");
    QTest::newRow("length") << 0;
    QTest::newRow("partial-timeout") << 1;
    QTest::newRow("rate") << 2;
    QTest::newRow("known-tag") << 3;
  }
  void malformedLengthPartialTimeoutAndFloodCloseTheStream() {
    QFETCH(int, kind);
    ds::test::TcpPeer peer;
    ds::TcpRobotLink link;
    QVERIFY(link.start(QHostAddress::LocalHost, peer.server.serverPort()));
    QTRY_VERIFY(link.ready());
    QTRY_VERIFY(peer.socket);
    if (kind == 0)
      peer.send(hex("ffff"));
    if (kind == 1)
      peer.send(hex("0010 0c"));
    if (kind == 2)
      peer.send(QByteArray(2 * (ds::TcpRobotLink::MaxFramesPerSecond + 1), '\0'));
    if (kind == 3)
      peer.send(hex("00010c"));
    QTRY_VERIFY_WITH_TIMEOUT(!link.active(), 2500);
    QVERIFY(!link.ready());
    QVERIFY(!link.lastError().isEmpty());
    QVERIFY(link.versions().isEmpty());
  }

  void writeBackpressureIsBoundedAndReconnectDiscardsOldPartialFrame() {
    ds::test::TcpPeer peer;
    ds::TcpRobotLink link;
    QVERIFY(link.start(QHostAddress::LocalHost, peer.server.serverPort()));
    QTRY_VERIFY(link.ready());
    QVector<ds::tcp::JoystickDescriptor> descriptors(6);
    for (int i = 0; i < 64 && link.active(); ++i)
      link.setStationData(descriptors, QString(ds::tcp::MaxGameData, QChar(u'A' + i)));
    QVERIFY(!link.active());
    QVERIFY(link.lastError().contains(QStringLiteral("queue")));
    QVERIFY(link.start(QHostAddress::LocalHost, peer.server.serverPort()));
    QTRY_VERIFY(link.ready());
    QTRY_COMPARE(peer.connections, 2);
    peer.send(hex("00"));
    QTest::qWait(20);
    link.stop();
    QVERIFY(link.start(QHostAddress::LocalHost, peer.server.serverPort()));
    QTRY_VERIFY(link.ready());
    QTRY_COMPARE(peer.connections, 3);
    peer.send(hex("00070c000000000001"));
    QTRY_COMPARE(link.receivedCount(), quint64(1));
    QVERIFY(link.ready());
  }

  void invalidEndpointsNeverStartTheSocket() {
    ds::TcpRobotLink link;
    for (const auto& address :
         {QHostAddress(QHostAddress::AnyIPv4), QHostAddress(QHostAddress::Broadcast),
          QHostAddress("224.0.0.1"), QHostAddress(QHostAddress::LocalHostIPv6)}) {
      QVERIFY(!link.start(address, 1740));
      QVERIFY(!link.active());
    }
    QVERIFY(!link.start(QHostAddress::LocalHost, 0));
    QVERIFY(!link.active());
  }
};

QTEST_GUILESS_MAIN(TcpTest)
#include "test_tcp.moc"
