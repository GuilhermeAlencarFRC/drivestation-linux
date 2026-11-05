#include "demo_session.h"
#include "main_window.h"
#include "practice_sequence.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <limits>

using Phase = ds::PracticeSequence::Phase;
using Mode = ds::DemoSession::Mode;

namespace {
void advanceSession(ds::DemoSession& session, qint64 milliseconds) {
  while (milliseconds > 0) {
    const auto step = std::min(milliseconds, ds::DemoSession::MaxTickGapMs);
    session.advance(step);
    milliseconds -= step;
  }
}

void prepareWindow(ds::MainWindow& window) {
  window.show();
  window.findChild<QTimer*>(QStringLiteral("demoTicker"))->stop();
  QApplication::processEvents();
  window.findChild<QCheckBox*>(QStringLiteral("simulateRobot"))->setChecked(true);
  QTest::mouseClick(window.findChild<QRadioButton*>(QStringLiteral("mode2")), Qt::LeftButton);
}
} // namespace

class PracticeTest : public QObject {
  Q_OBJECT
private slots:
  void exactBoundaries_data() {
    QTest::addColumn<qint64>("offset");
    QTest::addColumn<int>("phase");
    QTest::addColumn<bool>("enabled");
    QTest::addColumn<qint64>("remaining");
    const auto row = [](const char* name, qint64 time, Phase phase, bool enabled,
                        qint64 remaining) {
      QTest::newRow(name) << time << static_cast<int>(phase) << enabled << remaining;
    };
    row("start", 0, Phase::Countdown, false, 1000);
    row("countdown-last-ms", 999, Phase::Countdown, false, 1);
    row("auto-start", 1000, Phase::Autonomous, true, 2000);
    row("auto-last-ms", 2999, Phase::Autonomous, true, 1);
    row("delay-start", 3000, Phase::Delay, false, 1000);
    row("delay-last-ms", 3999, Phase::Delay, false, 1);
    row("teleop-start", 4000, Phase::Teleoperated, true, 3000);
    row("teleop-last-ms", 6999, Phase::Teleoperated, true, 1);
    row("endgame-start", 7000, Phase::EndGame, true, 1000);
    row("endgame-last-ms", 7999, Phase::EndGame, true, 1);
    row("complete", 8000, Phase::Complete, false, 0);
    row("huge-step", std::numeric_limits<qint64>::max(), Phase::Complete, false, 0);
    row("negative-time", -10, Phase::Countdown, false, 1000);
  }

  void exactBoundaries() {
    QFETCH(qint64, offset);
    QFETCH(int, phase);
    QFETCH(bool, enabled);
    QFETCH(qint64, remaining);
    ds::PracticeSequence sequence;
    QVERIFY(sequence.start({1, 2, 1, 3, 1}));
    sequence.advance(offset);
    QCOMPARE(static_cast<int>(sequence.phase()), phase);
    QCOMPARE(sequence.enabled(), enabled);
    QCOMPARE(sequence.remainingMs(), remaining);
    QCOMPARE(sequence.elapsedMs(), std::clamp<qint64>(offset, 0, 8000));
  }

  void zeroDurationSegmentsNeverLeaveAnActiveTerminalState() {
    for (int mask = 0; mask < 32; ++mask) {
      ds::PracticeSequence::Timings timings{};
      int positive = 0;
      for (int i = 0; i < 5; ++i) {
        timings[i] = (mask >> i) & 1;
        positive += timings[i];
      }
      ds::PracticeSequence sequence;
      QVERIFY(sequence.start(timings));
      if (positive == 0) {
        QCOMPARE(sequence.phase(), Phase::Complete);
        QVERIFY(!sequence.enabled());
      }
      int visits = 0;
      while (sequence.active() && visits < 6) {
        QVERIFY(sequence.remainingMs() > 0);
        sequence.advance(1000);
        ++visits;
      }
      QCOMPARE(visits, positive);
      QCOMPARE(sequence.elapsedMs(), positive * 1000);
      QCOMPARE(sequence.phase(), Phase::Complete);
      QVERIFY(!sequence.enabled());
      QVERIFY(!sequence.pendingEnable());
    }
  }

  void oneLargeStepMatchesPartitionedSteps() {
    ds::PracticeSequence large;
    ds::PracticeSequence small;
    QVERIFY(large.start({2, 3, 1, 5, 2}));
    QVERIFY(small.start({2, 3, 1, 5, 2}));
    large.advance(11234);
    for (int i = 0; i < 112; ++i)
      small.advance(100);
    small.advance(34);
    QCOMPARE(small.phase(), large.phase());
    QCOMPARE(small.elapsedMs(), large.elapsedMs());
    QCOMPARE(small.remainingMs(), large.remainingMs());
    QCOMPARE(small.enabled(), large.enabled());
  }

  void invalidTimingsAndRepeatedStartDoNotAlterTheCurrentRun() {
    ds::PracticeSequence sequence;
    QVERIFY(!sequence.start({-1, 1, 1, 1, 1}));
    QVERIFY(!sequence.start({1, 1, 601, 1, 1}));
    QCOMPARE(sequence.phase(), Phase::Idle);
    QVERIFY(sequence.start({1, 1, 1, 1, 1}));
    sequence.advance(500);
    QVERIFY(!sequence.start({0, 0, 0, 0, 0}));
    QCOMPARE(sequence.elapsedMs(), 500);
    QCOMPARE(sequence.phase(), Phase::Countdown);
  }

  void autonomousStopKeepsTheTimelineAndResumesAtTeleop() {
    ds::PracticeSequence sequence;
    QVERIFY(sequence.start({1, 2, 1, 3, 1}));
    QVERIFY(!sequence.autonomousStop());
    sequence.advance(1000);
    QVERIFY(sequence.autonomousStop());
    QVERIFY(!sequence.autonomousStop());
    QVERIFY(!sequence.enabled());
    QVERIFY(sequence.pendingEnable());
    sequence.advance(1999);
    QCOMPARE(sequence.phase(), Phase::Autonomous);
    QVERIFY(!sequence.enabled());
    sequence.advance(1);
    QCOMPARE(sequence.phase(), Phase::Delay);
    QVERIFY(sequence.pendingEnable());
    sequence.advance(1000);
    QCOMPARE(sequence.phase(), Phase::Teleoperated);
    QVERIFY(sequence.enabled());
    QVERIFY(!sequence.autoStopped());
    QVERIFY(!sequence.autonomousStop());
    sequence.advance(4000);
    QVERIFY(!sequence.enabled());
  }

  void pendingEnableRequiresAnEnabledSegmentInTheFuture() {
    ds::PracticeSequence sequence;
    QVERIFY(sequence.start({1, 0, 1, 0, 0}));
    QVERIFY(!sequence.pendingEnable());
    sequence.advance(1000);
    QCOMPARE(sequence.phase(), Phase::Delay);
    QVERIFY(!sequence.pendingEnable());
    sequence.cancel();
    QVERIFY(sequence.start({0, 1, 1, 0, 0}));
    QVERIFY(sequence.autonomousStop());
    QVERIFY(!sequence.pendingEnable());
  }

  void interruptionCancelsEveryPracticePhase_data() {
    QTest::addColumn<qint64>("offset");
    QTest::addColumn<bool>("aStop");
    QTest::addColumn<QString>("interruption");
    const std::array<qint64, 6> offsets{0, 1000, 2000, 3000, 4000, 1000};
    const QStringList interruptions{QStringLiteral("disable"), QStringLiteral("disconnect"),
                                    QStringLiteral("estop"), QStringLiteral("mode"),
                                    QStringLiteral("timing")};
    for (int i = 0; i < 6; ++i) {
      for (const auto& interruption : interruptions) {
        const auto name = QStringLiteral("phase%1-%2").arg(i).arg(interruption).toUtf8();
        QTest::newRow(name.constData()) << offsets[i] << (i == 5) << interruption;
      }
    }
  }

  void interruptionCancelsEveryPracticePhase() {
    QFETCH(qint64, offset);
    QFETCH(bool, aStop);
    QFETCH(QString, interruption);
    ds::DemoSession session;
    QVERIFY(session.setPracticeTimings({1, 1, 1, 1, 1}));
    session.setConnected(true);
    session.setMode(Mode::Practice);
    QVERIFY(session.enable());
    advanceSession(session, offset);
    if (aStop)
      QVERIFY(session.autonomousStop());
    QVERIFY(session.busy());
    if (interruption == QStringLiteral("disable"))
      session.disable();
    if (interruption == QStringLiteral("disconnect"))
      session.setConnected(false);
    if (interruption == QStringLiteral("estop"))
      session.emergencyStop();
    if (interruption == QStringLiteral("mode"))
      session.setMode(Mode::Teleoperated);
    if (interruption == QStringLiteral("timing"))
      QVERIFY(session.setPracticeTimings({2, 1, 1, 1, 1}));
    QVERIFY(!session.enabled());
    QVERIFY(!session.busy());
    QCOMPARE(session.practice().phase(), Phase::Cancelled);
    const auto stoppedAt = session.practice().elapsedMs();
    session.setConnected(true);
    advanceSession(session, 8000);
    QCOMPARE(session.practice().elapsedMs(), stoppedAt);
    QVERIFY(!session.enabled());
    QCOMPARE(session.emergencyStopped(), interruption == QStringLiteral("estop"));
  }

  void longClockGapsCancelRatherThanCatchUp() {
    for (int offset = 0; offset < 5000; offset += 1000) {
      ds::DemoSession session;
      session.setPracticeTimings({1, 1, 1, 1, 1});
      session.setConnected(true);
      session.setMode(Mode::Practice);
      session.enable();
      advanceSession(session, offset);
      session.advance(ds::DemoSession::MaxTickGapMs + 1);
      QCOMPARE(session.practice().phase(), Phase::Cancelled);
      QVERIFY(!session.enabled());
      advanceSession(session, 5000);
      QVERIFY(!session.enabled());
    }
  }

  void demoCannotRestartOrReplaceTimingWhileRunning() {
    ds::DemoSession session;
    session.setConnected(true);
    session.setMode(Mode::Practice);
    QVERIFY(session.enable());
    session.advance(500);
    QVERIFY(!session.canEnable());
    QVERIFY(!session.enable());
    QVERIFY(!session.setPracticeTimings({1, -1, 1, 1, 1}));
    QVERIFY(session.busy());
    QCOMPARE(session.elapsedMs(), 500);
    session.disable();
    QVERIFY(session.enable());
    QCOMPARE(session.elapsedMs(), 0);
  }

  void uiDisableCancelsTheCountdownBeforeAnyEnable() {
    QTemporaryDir directory;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath(QStringLiteral("countdown.ini"))));
    prepareWindow(window);
    auto* enable = window.findChild<QPushButton*>(QStringLiteral("enable"));
    auto* disable = window.findChild<QPushButton*>(QStringLiteral("disable"));
    QTest::mouseClick(enable, Qt::LeftButton);
    QVERIFY(!window.session().enabled());
    QVERIFY(disable->isEnabled());
    QVERIFY(!enable->isEnabled());
    QVERIFY(window.findChild<QLabel*>(QStringLiteral("dataSource"))
                ->text()
                .contains(QStringLiteral("WILL ENABLE")));
    QTest::keyClick(&window, Qt::Key_Return);
    QCOMPARE(window.session().practice().phase(), Phase::Cancelled);
    advanceSession(window.session(), 20000);
    QVERIFY(!window.session().enabled());
  }

  void uiBackspaceStopsAutoButStillAllowsEditingGameData() {
    QTemporaryDir directory;
    ds::PreferenceStore store(directory.filePath(QStringLiteral("astop.ini")));
    ds::Preferences preferences;
    preferences.practiceSeconds = {0, 2, 1, 2, 1};
    QVERIFY(store.save(preferences));
    ds::MainWindow window(store);
    prepareWindow(window);
    QTest::mouseClick(window.findChild<QPushButton*>(QStringLiteral("enable")), Qt::LeftButton);
    QVERIFY(window.session().enabled());
    window.selectTab(2);
    auto* data = window.findChild<QLineEdit*>(QStringLiteral("gameData"));
    data->setText(QStringLiteral("AB"));
    data->setCursorPosition(2);
    QTest::keyClick(data, Qt::Key_Backspace);
    QCOMPARE(data->text(), QStringLiteral("A"));
    QVERIFY(window.session().enabled());
    window.selectTab(0);
    auto* aStop = window.findChild<QPushButton*>(QStringLiteral("autonomousStop"));
    QVERIFY(aStop->isEnabled());
    QTest::keyClick(&window, Qt::Key_Backspace);
    QVERIFY(window.session().practice().autoStopped());
    QVERIFY(!window.session().enabled());
    QVERIFY(!aStop->isEnabled());
    advanceSession(window.session(), 3000);
    QVERIFY(window.session().enabled());
    QCOMPARE(window.session().practice().phase(), Phase::Teleoperated);
    QVERIFY(window.findChild<QRadioButton*>(QStringLiteral("mode2"))->isChecked());
  }

  void uiConfigurationChangesCancelPendingEnable_data() {
    QTest::addColumn<QString>("control");
    QTest::newRow("team") << QStringLiteral("teamNumber");
    QTest::newRow("station") << QStringLiteral("station");
    QTest::newRow("timing") << QStringLiteral("practice0");
  }

  void uiConfigurationChangesCancelPendingEnable() {
    QFETCH(QString, control);
    QTemporaryDir directory;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath(QStringLiteral("change.ini"))));
    prepareWindow(window);
    window.session().enable();
    if (auto* input = window.findChild<QSpinBox*>(control))
      input->setValue(input->value() + 1);
    else
      window.findChild<QComboBox*>(control)->setCurrentIndex(1);
    QCOMPARE(window.session().practice().phase(), Phase::Cancelled);
    advanceSession(window.session(), 20000);
    QVERIFY(!window.session().enabled());
  }

  void losingActivationAndClosingCancelPendingPractice() {
    QTemporaryDir directory;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath(QStringLiteral("close.ini"))));
    prepareWindow(window);
    window.session().enable();
    QEvent deactivate(QEvent::WindowDeactivate);
    QApplication::sendEvent(&window, &deactivate);
    QCOMPARE(window.session().practice().phase(), Phase::Cancelled);
    window.session().enable();
    window.close();
    QCOMPARE(window.session().practice().phase(), Phase::Cancelled);
    advanceSession(window.session(), 20000);
    QVERIFY(!window.session().enabled());
  }
};

QTEST_MAIN(PracticeTest)
#include "test_practice.moc"
