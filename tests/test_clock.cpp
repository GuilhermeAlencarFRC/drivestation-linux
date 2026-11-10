#include "clock_sync.h"
#include "udp_robot_link.h"

#include <QNetworkDatagram>
#include <QTimeZone>
#include <QtTest>
#include <algorithm>

namespace {
QByteArray hex(const char* text) { return QByteArray::fromHex(text); }
ds::wire::TimeData fixedTime() {
  QString error;
  return ds::makeTimeData(QDateTime(QDate(2026, 11, 9), QTime(18, 30, 45, 123),
                                    QTimeZone(QByteArrayLiteral("UTC"))),
                          "America/Cuiaba", error)
      .value();
}
class Peer : public QObject {
public:
  QUdpSocket socket;
  QVector<ds::wire::Control> controls;
  bool requestTime = false;
  bool replying = true;
  Peer() {
    socket.bind(QHostAddress::LocalHost, 0, QUdpSocket::DontShareAddress);
    connect(&socket, &QUdpSocket::readyRead, this, [this] {
      while (socket.hasPendingDatagrams()) {
        const auto datagram = socket.receiveDatagram();
        QString error;
        const auto control = ds::wire::decodeControl(datagram.data(), error);
        if (!control)
          continue;
        controls.append(*control);
        if (!replying)
          continue;
        ds::wire::Status status;
        status.sequence = control->sequence;
        status.trace = 0x30;
        status.enabled = control->enabled;
        status.request = requestTime ? 1 : 0;
        socket.writeDatagram(ds::wire::encodeStatus(status, error).value(),
                             datagram.senderAddress(), datagram.senderPort());
      }
    });
  }
  int timeCount() const {
    return static_cast<int>(
        std::count_if(controls.begin(), controls.end(),
                      [](const auto& control) { return control.time.has_value(); }));
  }
};
} // namespace

class ClockTest : public QObject {
  Q_OBJECT
private slots:
  void utcDateAndTimezoneHaveIndependentWireBytes() {
    ds::wire::Control control;
    control.sequence = 0x1234;
    control.time = fixedTime();
    QString error;
    const auto expected = hex("123401000000 0b0f0001e0782d1e12090a7e "
                              "0f10416d65726963612f437569616261");
    QCOMPARE(ds::wire::encodeControl(control, error).value(), expected);
    const auto decoded = ds::wire::decodeControl(expected, error);
    QVERIFY2(decoded, qPrintable(error));
    QVERIFY(decoded->time);
    QCOMPARE(decoded->time->microseconds, quint32(123000));
    QCOMPARE(decoded->time->second, quint8(45));
    QCOMPARE(decoded->time->minute, quint8(30));
    QCOMPARE(decoded->time->hour, quint8(18));
    QCOMPARE(decoded->time->day, quint8(9));
    QCOMPARE(decoded->time->month, quint8(10));
    QCOMPARE(decoded->time->yearSince1900, quint8(126));
    QCOMPARE(decoded->time->timezone, QByteArray("America/Cuiaba"));
    QCOMPARE(ds::wire::encodeControl(*decoded, error).value(), expected);
  }

  void localTimeConvertsToUtcWithoutAffectingMonotonicScheduling() {
    QString error;
    const auto local =
        QDateTime(QDate(2026, 11, 9), QTime(14, 30, 45, 123), QTimeZone(-4 * 60 * 60));
    const auto value = ds::makeTimeData(local, "America/Cuiaba", error);
    QVERIFY(value);
    QCOMPARE(value->hour, quint8(18));
    QCOMPARE(value->microseconds, quint32(123000));
    ds::ClockSync schedule;
    schedule.reset();
    QVERIFY(schedule.due(0));
    schedule.sent(0);
    QVERIFY(!schedule.due(100000)); // No periodic resend without a request.
    schedule.request();
    QVERIFY(!schedule.due(999));
    QVERIFY(schedule.due(1000));
    schedule.sent(1000);
    schedule.request();
    QVERIFY(!schedule.due(1500));
    QVERIFY(schedule.due(2000));
    schedule.reset();
    QVERIFY(schedule.due(0));
  }

  void invalidClockFieldsAndTimezonesAreRejected_data() {
    QTest::addColumn<int>("kind");
    for (int kind = 0; kind < 9; ++kind)
      QTest::newRow(qPrintable(QString::number(kind))) << kind;
  }
  void invalidClockFieldsAndTimezonesAreRejected() {
    QFETCH(int, kind);
    auto value = fixedTime();
    if (kind == 0)
      value.microseconds = 1000000;
    if (kind == 1)
      value.second = 60;
    if (kind == 2)
      value.minute = 60;
    if (kind == 3)
      value.hour = 24;
    if (kind == 4)
      value.month = 12;
    if (kind == 5) {
      value.yearSince1900 = 125;
      value.month = 1;
      value.day = 29;
    }
    if (kind == 6)
      value.timezone.clear();
    if (kind == 7)
      value.timezone.fill('A', ds::wire::MaxTimeZoneBytes + 1);
    if (kind == 8)
      value.timezone = QByteArray("UTC\0bad", 7);
    ds::wire::Control control;
    control.time = value;
    QString error;
    QVERIFY(!ds::wire::validTimeData(value));
    QVERIFY(!ds::wire::encodeControl(control, error));
    QVERIFY(!error.isEmpty());
  }

  void calendarRangeLeapDaysAndMicrosecondBoundary() {
    QString error;
    QVERIFY(!ds::makeTimeData(QDateTime(), "UTC", error));
    for (const int year : {1899, 2156})
      QVERIFY(!ds::makeTimeData(
          QDateTime(QDate(year, 1, 1), QTime(0, 0), QTimeZone(QByteArrayLiteral("UTC"))), "UTC",
          error));
    for (const int year : {1900, 2155})
      QVERIFY(ds::makeTimeData(
          QDateTime(QDate(year, 1, 1), QTime(0, 0), QTimeZone(QByteArrayLiteral("UTC"))), "UTC",
          error));
    auto value = ds::makeTimeData(QDateTime(QDate(2000, 2, 29), QTime(23, 59, 59),
                                            QTimeZone(QByteArrayLiteral("UTC"))),
                                  "UTC", error)
                     .value();
    value.microseconds = 999999;
    ds::wire::Control control;
    control.time = value;
    const auto packet = ds::wire::encodeControl(control, error);
    QVERIFY(packet);
    QCOMPARE(ds::wire::decodeControl(*packet, error)->time->microseconds, quint32(999999));
  }

  void truncatedDuplicateAndUnpairedTimeTagsAreRejected() {
    ds::wire::Control control;
    control.time = fixedTime();
    QString error;
    const auto packet = ds::wire::encodeControl(control, error).value();
    for (qsizetype size = 7; size < packet.size(); ++size)
      QVERIFY(!ds::wire::decodeControl(packet.left(size), error));
    const auto date = packet.mid(6, 12);
    const auto zone = packet.mid(18);
    for (const auto& malformed :
         {packet + date, packet + zone, packet.left(6) + zone, packet.left(6) + date})
      QVERIFY(!ds::wire::decodeControl(malformed, error));
    auto invalid = packet;
    invalid[9] = char(0xff); // Out-of-range microseconds, still a well-sized datagram.
    QVERIFY(!ds::wire::decodeControl(invalid, error));
  }

  void actualUdpRespondsToValidRequestsAtBoundedRateAndKeepsEnableExplicit() {
    Peer peer;
    QVERIFY(peer.socket.localPort());
    ds::UdpRobotLink link(nullptr,
                          [](QString&) { return std::optional<ds::wire::TimeData>(fixedTime()); });
    QVERIFY(
        link.start(QHostAddress::LocalHost, peer.socket.localPort(), 0, QHostAddress::LocalHost));
    QTRY_VERIFY(link.connection().connected());
    QTRY_COMPARE(peer.timeCount(), 1);
    QVERIFY(!peer.controls.first().enabled);
    QCOMPARE(peer.controls.first().time->timezone, QByteArray("America/Cuiaba"));
    QUdpSocket stranger;
    QVERIFY(stranger.bind(QHostAddress::LocalHost, 0));
    ds::wire::Status forged;
    forged.sequence = peer.controls.last().sequence;
    forged.trace = 0x30;
    forged.request = 1;
    QString error;
    stranger.writeDatagram(ds::wire::encodeStatus(forged, error).value(), QHostAddress::LocalHost,
                           link.localPort());
    QTest::qWait(1100);
    QCOMPARE(peer.timeCount(), 1); // Wrong source cannot schedule clock updates.
    QVERIFY(link.requestEnable());
    peer.requestTime = true;
    QTRY_COMPARE(peer.timeCount(), 2);
    QTest::qWait(200);
    QCOMPARE(peer.timeCount(), 2);
    QVERIFY(link.connection().enableRequested());
    QTRY_COMPARE_WITH_TIMEOUT(peer.timeCount(), 3, 1500);
    peer.requestTime = false;
    peer.replying = false;
    QTRY_VERIFY(!link.connection().connected());
    QVERIFY(!link.connection().enableRequested());
    peer.replying = true;
    QTRY_VERIFY(link.connection().connected());
    QTRY_VERIFY_WITH_TIMEOUT(peer.timeCount() >= 4, 1500);
    QVERIFY(!link.connection().enableRequested());
    QVERIFY(!peer.controls.last().enabled);
  }

  void unavailableClockNeverSendsEnable() {
    Peer peer;
    ds::UdpRobotLink link(nullptr, [](QString& error) -> std::optional<ds::wire::TimeData> {
      error = QStringLiteral("Clock unavailable for test");
      return {};
    });
    QVERIFY(
        !link.start(QHostAddress::LocalHost, peer.socket.localPort(), 0, QHostAddress::LocalHost));
    QVERIFY(!link.connection().active());
    QVERIFY(!link.connection().enableRequested());
    QVERIFY(!link.requestEnable());
    QVERIFY(link.lastError().contains(QStringLiteral("Clock unavailable")));
  }
};

QTEST_GUILESS_MAIN(ClockTest)
#include "test_clock.moc"
