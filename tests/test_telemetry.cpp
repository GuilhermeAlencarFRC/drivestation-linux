#include "telemetry_history.h"
#include "widgets.h"
#include <QJsonArray>
#include <QtTest>
#include <limits>

class TelemetryTest : public QObject {
  Q_OBJECT
private slots:
  void repliesAreSampledWithoutInventingValuesOrBridgingLoss() {
    ds::TelemetryHistory history;
    ds::wire::Status status;
    status.batteryRaw = 0x0c80;
    status.enabled = true;
    status.mode = ds::wire::Mode::Autonomous;
    status.outputs.append(ds::wire::JoystickOutput{});
    const auto first = history.append(0, 7, status);
    QVERIFY(first && first->startsSegment);
    QCOMPARE(first->status.voltage(), 12.5);
    QCOMPARE(first->roundTripMs, qint64(7));
    QVERIFY(first->status.outputs.isEmpty());
    QVERIFY(!history.append(100, 3, status));
    QVERIFY(history.append(200, 9, status));
    const auto continuous = history.append(449, 11, status);
    QVERIFY(continuous && !continuous->startsSegment);
    const auto resumed = history.append(699, 5, status);
    QVERIFY(resumed && resumed->startsSegment);
    QCOMPARE(resumed->segment, 1);
    QVERIFY(!history.append(698, 1, status));
    QVERIFY(!history.append(900, -1, status));
    const auto json = history.toJson().value("samples").toArray();
    QCOMPARE(json.size(), 4);
    QCOMPARE(json.last().toObject().value("battery_volts").toDouble(), 12.5);
    QVERIFY(json.first().toObject().value("enabled").toBool());
    history.breakSegment(); // A send-gap reset can happen before the response timeout.
    const auto sendGap = history.append(800, 2, status);
    QVERIFY(sendGap && sendGap->startsSegment);
    QCOMPARE(sendGap->segment, 2);
  }

  void boundedHistoryReportsDroppedDataAndTruncatedMessages() {
    ds::TelemetryHistory history;
    for (int i = 0; i < ds::TelemetryHistory::MaxSamples + 3; ++i)
      history.append(qint64(i) * 200, i % 20, {});
    ds::tcp::Message message;
    message.kind = ds::tcp::Message::Kind::Error;
    message.isError = true;
    message.text = QString(ds::TelemetryHistory::MaxTextChars + 10, QChar(u'á'));
    for (int i = 0; i < ds::TelemetryHistory::MaxMessages + 2; ++i)
      history.message(i, message);
    const auto json = history.toJson();
    QCOMPARE(history.samples().size(), size_t(ds::TelemetryHistory::MaxSamples));
    QCOMPARE(json.value("dropped_samples").toInteger(), qint64(3));
    QCOMPARE(json.value("dropped_messages").toInteger(), qint64(2));
    const auto messages = json.value("messages").toArray();
    QCOMPARE(messages.size(), ds::TelemetryHistory::MaxMessages);
    QVERIFY(messages.last().toObject().value("truncated").toBool());
    QCOMPARE(messages.last().toObject().value("text").toString().size(),
               qsizetype(ds::TelemetryHistory::MaxTextChars));
    history.clear();
    QVERIFY(history.samples().empty());
    QVERIFY(history.toJson().value("messages").toArray().isEmpty());
    QCOMPARE(history.toJson().value("dropped_samples").toInteger(), qint64(0));
  }

  void chartSeparatesSourcesAndRejectsInvalidCoordinates() {
    ds::TelemetryChart chart;
    chart.appendSample(0, 4, 12);
    QCOMPARE(chart.samples().size(), 1);
    chart.setSource(ds::TelemetryChart::Source::Network);
    QVERIFY(chart.samples().isEmpty());
    QVERIFY(chart.accessibleName().contains(QStringLiteral("Received UDP")));
    chart.appendSample(1, 65, 12.5, true);
    chart.appendSample(0.5, 1, 1);
    chart.appendSample(2, std::numeric_limits<double>::quiet_NaN(), 12);
    QCOMPARE(chart.samples().size(), 1);
    QVERIFY(chart.samples().first().startsSegment);
    QVERIFY(chart.toolTip().contains(QStringLiteral("12.50")));
    chart.advanceTime(3);
    chart.resize(400, 150);
    QVERIFY(!chart.grab().isNull());
    chart.setSource(ds::TelemetryChart::Source::Simulation);
    QVERIFY(chart.samples().isEmpty());
    QVERIFY(chart.accessibleName().contains(QStringLiteral("Simulated")));
  }
};

QTEST_MAIN(TelemetryTest)
#include "test_telemetry.moc"
