#include "robot_connection.h"
#include "udp_robot_link.h"

#include <QNetworkDatagram>
#include <QRandomGenerator>
#include <QtTest>

namespace {
QByteArray hex(const char* value) { return QByteArray::fromHex(value); }
ds::wire::Status statusFor(quint16 sequence) {
  ds::wire::Status status;
  status.sequence = sequence;
  status.trace = 0x30;
  status.batteryRaw = 0x0c80;
  return status;
}

class Peer : public QObject {
public:
  QUdpSocket socket;
  QVector<ds::wire::Control> controls;
  QNetworkDatagram last;
  bool replying = true;
  bool code = true;
  bool invalid = false;
  bool duplicate = false;
  QString error;
  Peer() {
    socket.bind(QHostAddress::LocalHost, 0, QUdpSocket::DontShareAddress);
    connect(&socket, &QUdpSocket::readyRead, this, [this] {
      while (socket.hasPendingDatagrams()) {
        last = socket.receiveDatagram();
        const auto control = ds::wire::decodeControl(last.data(), error);
        if (!control)
          continue;
        controls.append(*control);
        if (!replying)
          continue;
        auto status = statusFor(control->sequence);
        status.mode = control->mode;
        status.enabled = control->enabled;
        status.emergencyStopped = control->emergencyStopped;
        if (!code)
          status.trace = 0x10;
        auto packet = ds::wire::encodeStatus(status, error).value_or(QByteArray());
        if (invalid)
          packet[2] = 2;
        send(packet);
        if (duplicate)
          send(packet);
      }
    });
  }
  void send(const QByteArray& bytes) {
    socket.writeDatagram(bytes, last.senderAddress(), last.senderPort());
  }
};
} // namespace

class NetworkTest : public QObject {
  Q_OBJECT
private slots:
  void controlHeaderHasExplicitWireBytes_data() {
    QTest::addColumn<int>("mode");
    QTest::addColumn<bool>("enabled");
    QTest::addColumn<bool>("estop");
    QTest::addColumn<QByteArray>("expected");
    QTest::newRow("teleop-disabled") << 0 << false << false << hex("123401000005");
    QTest::newRow("test-enabled") << 1 << true << false << hex("123401050005");
    QTest::newRow("auto-enabled") << 2 << true << false << hex("123401060005");
    QTest::newRow("estop-overrides-enable") << 2 << true << true << hex("123401820005");
  }
  void controlHeaderHasExplicitWireBytes() {
    QFETCH(int, mode);
    QFETCH(bool, enabled);
    QFETCH(bool, estop);
    QFETCH(QByteArray, expected);
    ds::wire::Control control;
    control.sequence = 0x1234;
    control.mode = static_cast<ds::wire::Mode>(mode);
    control.enabled = enabled;
    control.emergencyStopped = estop;
    control.station = 5;
    QString error;
    QCOMPARE(ds::wire::encodeControl(control, error).value_or(QByteArray()), expected);
    const auto decoded = ds::wire::decodeControl(expected, error);
    QVERIFY2(decoded, qPrintable(error));
    QCOMPARE(decoded->sequence, quint16(0x1234));
    QCOMPARE(decoded->mode, control.mode);
    QCOMPARE(decoded->enabled, enabled && !estop);
    QCOMPARE(decoded->emergencyStopped, estop);
    QCOMPARE(decoded->station, quint8(5));
  }

  void joystickGoldenVectorPreservesBitsSignedAxesAndEmptySlots() {
    ds::wire::Control control;
    control.sequence = 0x1234;
    control.joysticks.resize(3);
    auto& joystick = control.joysticks[1];
    joystick.axes = {-128, 0, 127};
    joystick.buttons.resize(10);
    joystick.buttons[0] = joystick.buttons[9] = true;
    joystick.povs = {-1, 315};
    const auto expected = hex("123401000000 040c000000 0d0c0380007f0a020102ffff013b 040c000000");
    QString error;
    const auto packet = ds::wire::encodeControl(control, error);
    QVERIFY(packet);
    QCOMPARE(*packet, expected);
    const auto decoded = ds::wire::decodeControl(expected, error);
    QVERIFY2(decoded, qPrintable(error));
    QCOMPARE(decoded->joysticks.size(), 3);
    QVERIFY(decoded->joysticks[0].axes.isEmpty());
    QCOMPARE(decoded->joysticks[1].axes, joystick.axes);
    QCOMPARE(decoded->joysticks[1].buttons, joystick.buttons);
    QCOMPARE(decoded->joysticks[1].povs, joystick.povs);
    QVERIFY(decoded->joysticks[2].buttons.isEmpty());
  }

  void buttonMasksAcrossByteBoundaries_data() {
    QTest::addColumn<int>("count");
    QTest::addColumn<QByteArray>("mask");
    QTest::newRow("zero") << 0 << QByteArray();
    QTest::newRow("one") << 1 << hex("01");
    QTest::newRow("eight") << 8 << hex("81");
    QTest::newRow("nine") << 9 << hex("0101");
    QTest::newRow("sixteen") << 16 << hex("8001");
    QTest::newRow("seventeen") << 17 << hex("010001");
    QTest::newRow("thirty-two") << 32 << hex("80000001");
  }
  void buttonMasksAcrossByteBoundaries() {
    QFETCH(int, count);
    QFETCH(QByteArray, mask);
    ds::wire::Control control;
    ds::wire::Joystick joystick;
    joystick.buttons.resize(count);
    if (count)
      joystick.buttons[0] = joystick.buttons[count - 1] = true;
    control.joysticks = {joystick};
    QString error;
    const auto packet = ds::wire::encodeControl(control, error);
    QVERIFY(packet);
    QCOMPARE(packet->mid(10, mask.size()), mask);
    QCOMPARE(packet->size(), 11 + mask.size());
    const auto decoded = ds::wire::decodeControl(*packet, error);
    QVERIFY(decoded);
    QCOMPARE(decoded->joysticks[0].buttons, joystick.buttons);
  }

  void maximumJoystickProfileAndQuantization() {
    ds::wire::Control control;
    ds::wire::Joystick joystick;
    joystick.axes.fill(127, 12);
    joystick.buttons.fill(true, 32);
    joystick.povs.fill(359, 12);
    control.joysticks.fill(joystick, 6);
    QString error;
    auto packet = ds::wire::encodeControl(control, error);
    QVERIFY(packet);
    QVERIFY(packet->size() <= ds::wire::MaxDatagram);
    auto decoded = ds::wire::decodeControl(*packet, error);
    QVERIFY(decoded);
    QCOMPARE(decoded->joysticks.size(), 6);
    QCOMPARE(decoded->joysticks[5].buttons, joystick.buttons);
    QCOMPARE(ds::wire::quantizeAxis(-32768), qint8(-128));
    QCOMPARE(ds::wire::quantizeAxis(-1), qint8(-1));
    QCOMPARE(ds::wire::quantizeAxis(0), qint8(0));
    QCOMPARE(ds::wire::quantizeAxis(255), qint8(0));
    QCOMPARE(ds::wire::quantizeAxis(256), qint8(1));
    QCOMPARE(ds::wire::quantizeAxis(32767), qint8(127));
    control.joysticks.append(joystick);
    QVERIFY(!ds::wire::encodeControl(control, error));
    control.joysticks.resize(1);
    control.joysticks[0].axes.append(0);
    QVERIFY(!ds::wire::encodeControl(control, error));
    control.joysticks[0] = joystick;
    control.joysticks[0].buttons.append(false);
    QVERIFY(!ds::wire::encodeControl(control, error));
    control.joysticks[0] = joystick;
    control.joysticks[0].povs.append(0);
    QVERIFY(!ds::wire::encodeControl(control, error));
    control.joysticks[0] = joystick;
    control.joysticks[0].povs[0] = 360;
    QVERIFY(!ds::wire::encodeControl(control, error));
    control.joysticks.clear();
    control.mode = static_cast<ds::wire::Mode>(3);
    QVERIFY(!ds::wire::encodeControl(control, error));
    control.mode = ds::wire::Mode::Teleoperated;
    control.station = 6;
    QVERIFY(!ds::wire::encodeControl(control, error));
  }

  void statusGoldenVectorAndJoystickOutputs() {
    QString error;
    const auto expected = hex("12340116300c8001 0101 090112345678aabbccdd 0299fe");
    auto status = ds::wire::decodeStatus(expected, error);
    QVERIFY2(status, qPrintable(error));
    QCOMPARE(status->sequence, quint16(0x1234));
    QCOMPARE(status->mode, ds::wire::Mode::Autonomous);
    QVERIFY(status->enabled);
    QVERIFY(status->brownout);
    QVERIFY(status->hasRobotCode());
    QCOMPARE(status->voltage(), 12.5);
    QCOMPARE(status->request, quint8(1));
    QCOMPARE(status->outputs.size(), 2);
    QCOMPARE(status->outputs[0].digital, quint32(0));
    QCOMPARE(status->outputs[1].digital, quint32(0x12345678));
    QCOMPARE(status->outputs[1].leftRumble, quint16(0xaabb));
    QCOMPARE(status->outputs[1].rightRumble, quint16(0xccdd));
    QCOMPARE(ds::wire::encodeStatus(*status, error).value_or(QByteArray()),
             expected.left(expected.size() - 3));
    status->batteryRaw = 0xffff;
    QCOMPARE(status->voltage(), 255.99609375);
    status->emergencyStopped = true;
    status->codeInitializing = true;
    const auto encoded = ds::wire::encodeStatus(*status, error);
    QVERIFY(encoded);
    auto stopped = ds::wire::decodeStatus(*encoded, error);
    QVERIFY(stopped && stopped->emergencyStopped && stopped->codeInitializing);
    QVERIFY(!stopped->enabled);
  }

  void malformedDatagramsAreRejected_data() {
    QTest::addColumn<bool>("status");
    QTest::addColumn<QByteArray>("packet");
    QTest::newRow("control-short") << false << hex("0000010000");
    QTest::newRow("status-short") << true << hex("00000100300c80");
    QTest::newRow("wrong-version") << true << hex("00000200300c8000");
    QTest::newRow("bad-mode") << false << hex("000001030000");
    QTest::newRow("fms-outside-profile") << false << hex("000001080000");
    QTest::newRow("reboot-outside-profile") << false << hex("000001000800");
    QTest::newRow("station") << false << hex("000001000006");
    QTest::newRow("estop-and-enable") << false << hex("000001840000");
    QTest::newRow("zero-tag-length") << true << hex("00000100300c8000 00");
    QTest::newRow("truncated-tag") << true << hex("00000100300c8000 090100");
    QTest::newRow("short-joystick") << false << hex("000001000000 010c");
    QTest::newRow("short-axes") << false << hex("000001000000 040c020000");
    QTest::newRow("short-buttons") << false << hex("000001000000 040c000900");
    QTest::newRow("unused-button-bits") << false << hex("000001000000 050c00010200");
    QTest::newRow("short-povs") << false << hex("000001000000 040c000001");
    QTest::newRow("invalid-pov") << false << hex("000001000000 060c0000010168");
    QTest::newRow("extra-joystick-data") << false << hex("000001000000 050c00000000");
    QTest::newRow("bad-output-tag") << true << hex("00000100300c8000 020100");
    QTest::newRow("reserved-status") << true << hex("00000140300c8000");
    QTest::newRow("oversized") << true << QByteArray(1025, '\0');
    QTest::newRow("seven-joysticks")
        << false << hex("000001000000") + hex("040c000000").repeated(7);
    QTest::newRow("seven-outputs") << true << hex("00000100300c8000") + hex("0101").repeated(7);
  }
  void malformedDatagramsAreRejected() {
    QFETCH(bool, status);
    QFETCH(QByteArray, packet);
    QString error;
    const bool valid = status ? ds::wire::decodeStatus(packet, error).has_value()
                              : ds::wire::decodeControl(packet, error).has_value();
    QVERIFY(!valid);
    QVERIFY(!error.isEmpty());
  }

  void unknownTagsAndEveryBytePrefixAreBounded() {
    QString error;
    QVERIFY(ds::wire::decodeControl(hex("000001000000 01fe"), error));
    QVERIFY(ds::wire::decodeStatus(hex("00000100300c8000 01fe"), error));
    const auto packet = hex("000001000000 0d0c0380007f0a020102ffff013b");
    for (int size = 1; size < 6; ++size)
      QVERIFY(!ds::wire::decodeControl(packet.left(size), error));
    // A header alone is valid; a partial tag is never valid.
    for (qsizetype size = 7; size < packet.size(); ++size)
      QVERIFY2(!ds::wire::decodeControl(packet.left(size), error),
               qPrintable(QString::number(size)));
  }

  void deterministicMalformedInputSweep() {
    QRandomGenerator random(0x141501);
    QString error;
    for (int sample = 0; sample < 10000; ++sample) {
      QByteArray bytes(static_cast<int>(random.bounded(1050u)), Qt::Uninitialized);
      for (auto& value : bytes)
        value = static_cast<char>(random.generate() & 0xff);
      if (bytes.size() >= 3 && sample % 2 == 0)
        bytes[2] = 1;
      const auto control = ds::wire::decodeControl(bytes, error);
      if (control) {
        QVERIFY(control->joysticks.size() <= 6);
        QVERIFY(ds::wire::encodeControl(*control, error));
      }
      const auto status = ds::wire::decodeStatus(bytes, error);
      if (status) {
        QVERIFY(status->outputs.size() <= 6);
        QVERIFY(ds::wire::encodeStatus(*status, error));
      }
    }
  }

  void connectionRequiresFreshCodeAndExplicitEnable() {
    ds::RobotConnection state;
    QVERIFY(!state.requestEnable(0));
    state.start(10, 0);
    QVERIFY(!state.requestEnable(0));
    QVERIFY(!state.nextControl(0).enabled);
    auto status = statusFor(10);
    status.trace = 0x10;
    QVERIFY(state.accept(status, 2));
    QVERIFY(!state.requestEnable(2));
    auto sent = state.nextControl(20);
    status.sequence = sent.sequence;
    status.trace = 0x30;
    status.codeInitializing = true;
    QVERIFY(state.accept(status, 21));
    QVERIFY(!state.requestEnable(21));
    sent = state.nextControl(40);
    status.sequence = sent.sequence;
    status.codeInitializing = false;
    QVERIFY(state.accept(status, 42));
    QVERIFY(state.requestEnable(42));
    QVERIFY(state.nextControl(60).enabled);
    state.disable();
    QVERIFY(!state.nextControl(80).enabled);
  }

  void sequenceWrapDuplicatesAndReorderingDoNotExtendLiveness() {
    ds::RobotConnection state;
    state.start(65534, 0);
    QCOMPARE(state.nextControl(0).sequence, quint16(65534));
    QCOMPARE(state.nextControl(20).sequence, quint16(65535));
    QCOMPARE(state.nextControl(40).sequence, quint16(0));
    QVERIFY(state.accept(statusFor(65535), 43));
    QVERIFY(!state.accept(statusFor(65534), 44));
    QVERIFY(state.accept(statusFor(0), 45));
    QCOMPARE(state.roundTripMs(), std::optional<qint64>(5));
    QVERIFY(!state.accept(statusFor(0), 60));
    QVERIFY(!state.accept(statusFor(1000), 61));
    for (qint64 now = 60; now < 295; now += 20)
      state.nextControl(now);
    state.expire(294);
    QVERIFY(state.connected());
    state.expire(295);
    QVERIFY(!state.connected());
    QVERIFY(!state.roundTripMs());
  }

  void lossAndRecoveryNeverImplicitlyEnable() {
    ds::RobotConnection state;
    state.start(1, 0);
    state.nextControl(0);
    QVERIFY(state.accept(statusFor(1), 0));
    QVERIFY(state.requestEnable(0));
    for (int now = 20; now <= 240; now += 20)
      QVERIFY(state.nextControl(now).enabled);
    // accept() may receive a value borrowed from its own exposed status snapshot.
    QVERIFY(!state.accept(*state.status(), 250));
    QVERIFY(!state.connected());
    QVERIFY(!state.enableRequested());
    QVERIFY(!state.accept(statusFor(1), 251));
    const auto reconnect = state.nextControl(260);
    QVERIFY(!reconnect.enabled);
    QVERIFY(state.accept(statusFor(reconnect.sequence), 261));
    QVERIFY(state.connected());
    QVERIFY(!state.enableRequested());
    QVERIFY(!state.nextControl(280).enabled);
    QVERIFY(state.requestEnable(281));
    QVERIFY(state.nextControl(300).enabled);
  }

  void clockGapCodeLossAndConfigurationDisable_data() {
    QTest::addColumn<int>("cause");
    QTest::newRow("clock-gap") << 0;
    QTest::newRow("code-loss") << 1;
    QTest::newRow("initializing") << 2;
    QTest::newRow("mode-change") << 3;
    QTest::newRow("station-change") << 4;
    QTest::newRow("invalid-input") << 5;
    QTest::newRow("restart") << 6;
    QTest::newRow("invalid-mode") << 7;
    QTest::newRow("invalid-station") << 8;
  }
  void clockGapCodeLossAndConfigurationDisable() {
    QFETCH(int, cause);
    ds::RobotConnection state;
    state.start(1, 0);
    state.nextControl(0);
    QVERIFY(state.accept(statusFor(1), 1));
    QVERIFY(state.requestEnable(1));
    const auto sent = state.nextControl(20);
    QVERIFY(sent.enabled);
    switch (cause) {
    case 0:
      QVERIFY(!state.nextControl(120).enabled);
      break;
    case 1:
    case 2: {
      auto status = statusFor(sent.sequence);
      if (cause == 1)
        status.trace = 0x10;
      else
        status.codeInitializing = true;
      QVERIFY(state.accept(status, 21));
      break;
    }
    case 3:
      QVERIFY(state.setMode(ds::wire::Mode::Autonomous));
      break;
    case 4:
      QVERIFY(state.setStation(3));
      break;
    case 5: {
      QString error;
      QVERIFY(!state.setJoysticks(QVector<ds::wire::Joystick>(7), error));
      break;
    }
    case 6:
      state.stop();
      state.start(123, 21);
      break;
    case 7:
      QVERIFY(!state.setMode(static_cast<ds::wire::Mode>(3)));
      break;
    case 8:
      QVERIFY(!state.setStation(6));
      break;
    }
    QVERIFY(!state.enableRequested());
  }

  void emergencyStopIsLatchedAcrossRestart_data() {
    QTest::addColumn<bool>("remote");
    QTest::newRow("local") << false;
    QTest::newRow("remote") << true;
  }
  void emergencyStopIsLatchedAcrossRestart() {
    QFETCH(bool, remote);
    ds::RobotConnection state;
    state.start(1, 0);
    state.nextControl(0);
    auto status = statusFor(1);
    status.emergencyStopped = remote;
    QVERIFY(state.accept(status, 1));
    if (!remote) {
      QVERIFY(state.requestEnable(1));
      state.emergencyStop();
    }
    QVERIFY(state.emergencyStopped());
    QVERIFY(!state.requestEnable(1));
    auto outgoing = state.nextControl(20);
    QVERIFY(outgoing.emergencyStopped && !outgoing.enabled);
    state.stop();
    state.start(100, 21);
    outgoing = state.nextControl(21);
    QVERIFY(outgoing.emergencyStopped && !outgoing.enabled);
    QVERIFY(state.accept(statusFor(100), 22));
    QVERIFY(!state.requestEnable(22));
  }

  void pendingQueueSurvivesLongSilenceAndManySequenceWraps() {
    ds::RobotConnection state;
    state.start(0, 0);
    state.nextControl(0);
    QVERIFY(state.accept(statusFor(0), 1));
    ds::wire::Control sent;
    for (qint64 i = 1; i < 140000; ++i)
      sent = state.nextControl(i * 20);
    QVERIFY(!state.connected());
    QVERIFY(state.accept(statusFor(sent.sequence), 139999 * 20 + 1));
    QVERIFY(state.connected());
    QVERIFY(!state.enableRequested());
  }

  void realUdpExchangeEnableStopAndRestart() {
    Peer peer;
    QVERIFY(peer.socket.localPort());
    ds::UdpRobotLink link;
    QVERIFY(
        link.start(QHostAddress::LocalHost, peer.socket.localPort(), 0, QHostAddress::LocalHost));
    QTRY_VERIFY(link.connection().connected());
    QCOMPARE(link.connection().status()->voltage(), 12.5);
    QVERIFY(!link.connection().enableRequested());
    QVERIFY(link.requestEnable());
    QTRY_VERIFY(link.connection().status() && link.connection().status()->enabled);
    link.disable();
    QTRY_VERIFY(link.connection().status() && !link.connection().status()->enabled);
    QVERIFY(link.setMode(ds::wire::Mode::Test));
    QVERIFY(link.setStation(4));
    QTRY_VERIFY(!peer.controls.isEmpty() && peer.controls.last().station == 4);
    QCOMPARE(peer.controls.last().mode, ds::wire::Mode::Test);
    QVERIFY(link.requestEnable());
    QTRY_VERIFY(peer.controls.last().enabled);
    link.stop();
    QVERIFY(!link.connection().active());
    QTRY_VERIFY(!peer.controls.last().enabled);
    QVERIFY(
        link.start(QHostAddress::LocalHost, peer.socket.localPort(), 0, QHostAddress::LocalHost));
    QTRY_VERIFY(link.connection().connected());
    QVERIFY(!link.connection().enableRequested());
    QVERIFY(!link.connection().status()->enabled);
    QVERIFY(link.sentCount() > 0 && link.receivedCount() > 0);
  }

  void corruptAndWrongSourceDatagramsDoNotFeedWatchdog() {
    Peer peer;
    ds::UdpRobotLink link;
    QVERIFY(
        link.start(QHostAddress::LocalHost, peer.socket.localPort(), 0, QHostAddress::LocalHost));
    QTRY_VERIFY(link.connection().connected());
    QVERIFY(link.requestEnable());
    peer.invalid = true;
    QUdpSocket stranger;
    QVERIFY(stranger.bind(QHostAddress::LocalHost, 0, QUdpSocket::DontShareAddress));
    const auto response =
        ds::wire::encodeStatus(statusFor(peer.controls.last().sequence), peer.error);
    QVERIFY(response);
    stranger.writeDatagram(*response, QHostAddress::LocalHost, link.localPort());
    peer.send(QByteArray(ds::wire::MaxDatagram + 1, '\0'));
    QTRY_VERIFY_WITH_TIMEOUT(!link.connection().connected(), 1500);
    QVERIFY(!link.connection().enableRequested());
    QVERIFY(link.rejectedCount() >= 3);
    peer.invalid = false;
    peer.duplicate = true;
    QTRY_VERIFY(link.connection().connected());
    QVERIFY(!link.connection().enableRequested());
    QTRY_VERIFY(link.rejectedCount() > 3);
  }

  void sourceAddressAndSourcePortMustMatchTheConfiguredPeer() {
    Peer peer;
    peer.replying = false;
    ds::UdpRobotLink link;
    QVERIFY(
        link.start(QHostAddress::LocalHost, peer.socket.localPort(), 0, QHostAddress::LocalHost));
    QTRY_VERIFY(!peer.controls.isEmpty());
    QUdpSocket wrongPort;
    QVERIFY(wrongPort.bind(QHostAddress::LocalHost, 0, QUdpSocket::DontShareAddress));
    QString error;
    auto response = ds::wire::encodeStatus(statusFor(peer.controls.last().sequence), error);
    QVERIFY(response);
    wrongPort.writeDatagram(*response, QHostAddress::LocalHost, link.localPort());
    QTRY_VERIFY(link.rejectedCount() >= 1);
    QCOMPARE(link.receivedCount(), quint64(0));
    QVERIFY(!link.connection().connected());
    QUdpSocket wrongAddress;
    QVERIFY(wrongAddress.bind(QHostAddress(QStringLiteral("127.0.0.2")), peer.socket.localPort(),
                              QUdpSocket::DontShareAddress));
    response = ds::wire::encodeStatus(statusFor(peer.controls.last().sequence), error);
    wrongAddress.writeDatagram(*response, QHostAddress::LocalHost, link.localPort());
    QTRY_VERIFY(link.rejectedCount() >= 2);
    QCOMPARE(link.receivedCount(), quint64(0));
    response = ds::wire::encodeStatus(statusFor(peer.controls.last().sequence), error);
    peer.send(*response);
    QTRY_VERIFY(link.connection().connected());
    QCOMPARE(link.receivedCount(), quint64(1));
  }

  void repeatedPreviouslyAcceptedReplyCannotKeepTheLinkAlive() {
    Peer peer;
    ds::UdpRobotLink link;
    QVERIFY(
        link.start(QHostAddress::LocalHost, peer.socket.localPort(), 0, QHostAddress::LocalHost));
    QTRY_VERIFY(link.connection().connected());
    QVERIFY(link.requestEnable());
    QTRY_VERIFY(link.connection().status() && link.connection().status()->enabled);
    peer.replying = false;
    QString error;
    const auto old = ds::wire::encodeStatus(*link.connection().status(), error);
    QVERIFY(old);
    QTimer replay;
    replay.setInterval(20);
    connect(&replay, &QTimer::timeout, &peer, [&] { peer.send(*old); });
    replay.start();
    QTRY_VERIFY_WITH_TIMEOUT(!link.connection().connected(), 1500);
    QVERIFY(!link.connection().enableRequested());
    QVERIFY(link.rejectedCount() > 0);
    replay.stop();
    peer.replying = true;
    QTRY_VERIFY(link.connection().connected());
    QVERIFY(!link.connection().enableRequested());
  }

  void occupiedPortAndInvalidEndpointFailWithoutOpeningANewLink() {
    Peer peer;
    ds::UdpRobotLink link;
    QVERIFY(!link.start(QHostAddress::LocalHost, peer.socket.localPort(), peer.socket.localPort(),
                        QHostAddress::LocalHost));
    QVERIFY(!link.lastError().isEmpty());
    QVERIFY(!link.connection().active());
    QVERIFY(!link.start(QHostAddress::Broadcast, 1110, 0));
    QVERIFY(!link.start(QHostAddress::AnyIPv4, 1110, 0));
    QVERIFY(!link.start(QHostAddress::LocalHostIPv6, 1110, 0));
    QVERIFY(!link.start(QHostAddress::LocalHost, 0, 0));
    QVERIFY(!link.requestEnable());
    QCOMPARE(link.localPort(), quint16(0));
  }
};

QTEST_GUILESS_MAIN(NetworkTest)
#include "test_network.moc"
