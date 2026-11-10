#include "demo_session.h"
#include "main_window.h"
#include "preferences.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

class DriveStationTest : public QObject {
  Q_OBJECT
private slots:
  void startupIsDisconnected() {
    ds::DemoSession session;
    QVERIFY(!session.connected());
    QVERIFY(!session.enabled());
    QVERIFY(!session.enable());
    QCOMPARE(session.status(), QStringLiteral("No Robot Communication"));
    QCOMPARE(session.voltage(), 0.0);
  }

  void reconnectAndModeChangeRequireExplicitEnable() {
    ds::DemoSession session;
    session.setConnected(true);
    QVERIFY(!session.enabled());
    QVERIFY(session.enable());
    session.advance(800);
    session.advance(450);
    QCOMPARE(session.elapsedMs(), 1250);
    session.setConnected(false);
    session.advance(500);
    QVERIFY(!session.enabled());
    QCOMPARE(session.elapsedMs(), 1250);
    session.setConnected(true);
    QVERIFY(!session.enabled());
    QVERIFY(session.enable());
    QCOMPARE(session.elapsedMs(), 0);
    session.setMode(ds::DemoSession::Mode::Autonomous);
    QVERIFY(!session.enabled());
    session.setMode(ds::DemoSession::Mode::Practice);
    QVERIFY(session.enable());
    QVERIFY(session.busy());
    QVERIFY(!session.enabled());
    session.setMode(ds::DemoSession::Mode::Test);
    QVERIFY(session.enable());
    session.advance(-1);
    QCOMPARE(session.elapsedMs(), 0);
  }

  void emergencyStopSurvivesReconnectAndModeChange() {
    ds::DemoSession session;
    session.setConnected(true);
    session.enable();
    session.emergencyStop();
    QVERIFY(session.emergencyStopped());
    QVERIFY(!session.enabled());
    QVERIFY(!session.enable());
    session.setConnected(false);
    session.setConnected(true);
    session.setMode(ds::DemoSession::Mode::Autonomous);
    QVERIFY(!session.enable());
    session.reset();
    QVERIFY(!session.connected());
    QVERIFY(!session.emergencyStopped());
    QVERIFY(!session.enable());
    session.setConnected(true);
    QVERIFY(session.enable());
  }

  void preferencesRoundTripAndRejectInvalidValues() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ds::PreferenceStore store(directory.filePath(QStringLiteral("settings.ini")));
    ds::Preferences input;
    input.team = 10327;
    input.station = 4;
    input.dashboard = 3;
    input.gameData = QStringLiteral("FRC test á <&>");
    input.practiceSeconds = {3, 15, 2, 105, 20};
    QVERIFY(store.save(input));
    const auto output = store.load();
    QCOMPARE(output.team, input.team);
    QCOMPARE(output.station, input.station);
    QCOMPARE(output.dashboard, input.dashboard);
    QCOMPARE(output.gameData, input.gameData);
    QCOMPARE(output.practiceSeconds, input.practiceSeconds);
    QSettings settings(store.fileName(), QSettings::IniFormat);
    QVERIFY(!settings.contains(QStringLiteral("enabled")));
    QVERIFY(!settings.contains(QStringLiteral("connected")));
    settings.setValue(QStringLiteral("team"), QStringLiteral("not a number"));
    settings.setValue(QStringLiteral("station"), 999);
    settings.setValue(QStringLiteral("practice/0"), -5);
    settings.sync();
    const auto recovered = store.load();
    QCOMPARE(recovered.team, 0);
    QCOMPARE(recovered.station, 0);
    QCOMPARE(recovered.practiceSeconds[0], 5);
  }

  void configurationWriteFailureIsReported() {
    QTemporaryDir directory;
    QFile parentFile(directory.filePath(QStringLiteral("not-a-directory")));
    QVERIFY(parentFile.open(QIODevice::WriteOnly));
    parentFile.close();
    ds::PreferenceStore store(parentFile.fileName() + QStringLiteral("/settings.ini"));
    QVERIFY(!store.save(ds::Preferences{}));
  }

  void allTabsAndRightViewsNavigate() {
    QTemporaryDir directory;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath(QStringLiteral("ui.ini"))));
    window.show();
    QTest::qWait(10);
    QVERIFY(window.windowTitle().contains(QStringLiteral("VERSÃO DE TESTES")));
    QVERIFY(window.windowTitle().contains(QStringLiteral(DRIVESTATION_VERSION)));
    auto* alphaLabel = window.findChild<QLabel*>(QStringLiteral("dataSource"));
    QVERIFY(alphaLabel && alphaLabel->isVisible());
    QVERIFY(alphaLabel->text().contains(QStringLiteral("ALFA")));
    auto* pages = window.findChild<QStackedWidget*>(QStringLiteral("pages"));
    QVERIFY(pages);
    for (int i = 0; i < 5; ++i) {
      auto* tab = window.findChild<QToolButton*>(QStringLiteral("tab%1").arg(i));
      QVERIFY(tab);
      QTest::mouseClick(tab, Qt::LeftButton);
      QCOMPARE(pages->currentIndex(), i);
    }
    auto* messages = window.findChild<QWidget*>(QStringLiteral("messagePanel"));
    auto* charts = window.findChild<QWidget*>(QStringLiteral("chartPanel"));
    for (int view = 0; view < 3; ++view) {
      QTest::mouseClick(window.findChild<QToolButton*>(QStringLiteral("view%1").arg(view)),
                        Qt::LeftButton);
      QCOMPARE(messages->isVisible(), view != 1);
      QCOMPARE(charts->isVisible(), view != 0);
    }
    window.resize(1024, 224);
    QTest::qWait(10);
    QCOMPARE(window.width(), 1024);
    QCOMPARE(window.height(), 200);
  }

  void uiPreferencesSurviveRestartWithoutConnectionState() {
    QTemporaryDir directory;
    ds::PreferenceStore store(directory.filePath(QStringLiteral("ui.ini")));
    {
      ds::MainWindow window(store);
      window.findChild<QSpinBox*>(QStringLiteral("teamNumber"))->setValue(10327);
      window.findChild<QComboBox*>(QStringLiteral("dashboard"))->setCurrentIndex(2);
      window.findChild<QLineEdit*>(QStringLiteral("gameData"))
          ->setText(QStringLiteral("TEST DATA"));
      window.findChild<QCheckBox*>(QStringLiteral("simulateRobot"))->setChecked(true);
      window.session().enable();
      QCOMPARE(window.findChild<QLabel*>(QStringLiteral("teamStatus"))->text(),
               QStringLiteral("10327"));
    }
    ds::MainWindow reopened(store);
    QCOMPARE(reopened.findChild<QSpinBox*>(QStringLiteral("teamNumber"))->value(), 10327);
    QCOMPARE(reopened.findChild<QComboBox*>(QStringLiteral("dashboard"))->currentIndex(), 2);
    QCOMPARE(reopened.findChild<QLineEdit*>(QStringLiteral("gameData"))->text(),
             QStringLiteral("TEST DATA"));
    QVERIFY(!reopened.session().connected());
    QVERIFY(!reopened.session().enabled());
  }

  void numericEditorsKeepEnoughRoomToDisplayTheirValues() {
    QTemporaryDir directory;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath(QStringLiteral("numeric.ini"))));
    window.selectTab(2);
    window.show();
    QTest::qWait(10);
    for (auto* spin : window.findChildren<QSpinBox*>()) {
      auto* editor = spin->findChild<QLineEdit*>();
      QVERIFY(editor);
      QVERIFY2(editor->width() >= 28, qPrintable(spin->objectName()));
      QVERIFY(editor->isVisible());
      QVERIFY(!editor->text().isEmpty());
    }
  }

  void keyboardDisableStopAndResetAreApplied() {
    QTemporaryDir directory;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath(QStringLiteral("keys.ini"))));
    window.show();
    QTest::qWait(10);
    auto* simulate = window.findChild<QCheckBox*>(QStringLiteral("simulateRobot"));
    auto* enable = window.findChild<QPushButton*>(QStringLiteral("enable"));
    QVERIFY(!enable->isEnabled());
    simulate->setChecked(true);
    QVERIFY(enable->isEnabled());
    QTest::mouseClick(enable, Qt::LeftButton);
    QVERIFY(window.session().enabled());
    QTest::keyClick(enable, Qt::Key_Return);
    QVERIFY(!window.session().enabled());
    QTest::mouseClick(enable, Qt::LeftButton);
    QTest::keyClick(enable, Qt::Key_Space);
    QVERIFY(window.session().emergencyStopped());
    QVERIFY(!window.session().enabled());
    QVERIFY(!enable->isEnabled());
    QTest::mouseClick(window.findChild<QToolButton*>(QStringLiteral("consoleSettings")),
                      Qt::LeftButton);
    QApplication::processEvents();
    auto* reset = window.findChild<QPushButton*>(QStringLiteral("resetDemo"));
    QVERIFY(reset->isVisible());
    QTest::mouseClick(reset, Qt::LeftButton);
    window.findChild<QMenu*>(QStringLiteral("consoleMenu"))->hide();
    QVERIFY(!window.session().connected());
    QVERIFY(!window.session().emergencyStopped());
    QVERIFY(!simulate->isChecked());
  }

  void chordOnlyEnablesWithThreeHeldKeysOutsideTextFields() {
    QTemporaryDir directory;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath(QStringLiteral("chord.ini"))));
    window.show();
    window.session().setConnected(true);
    window.setFocus();
    QTest::keyClick(&window, Qt::Key_BracketLeft);
    QTest::keyClick(&window, Qt::Key_BracketRight);
    QTest::keyClick(&window, Qt::Key_Backslash);
    QVERIFY(!window.session().enabled());
    QTest::keyPress(&window, Qt::Key_BracketLeft);
    QTest::keyPress(&window, Qt::Key_BracketRight);
    QTest::keyPress(&window, Qt::Key_Backslash);
    QVERIFY(window.session().enabled());
    QTest::keyRelease(&window, Qt::Key_BracketLeft);
    QTest::keyRelease(&window, Qt::Key_BracketRight);
    QTest::keyRelease(&window, Qt::Key_Backslash);
    window.session().disable();
    window.selectTab(2);
    auto* data = window.findChild<QLineEdit*>(QStringLiteral("gameData"));
    QTest::keyClicks(data, QStringLiteral("[]\\"));
    QVERIFY(!window.session().enabled());
    QCOMPARE(data->text(), QStringLiteral("[]\\"));
  }

  void losingWindowActivationDisablesDemo() {
    QTemporaryDir directory;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath(QStringLiteral("focus.ini"))));
    window.session().setConnected(true);
    window.session().enable();
    QEvent deactivate(QEvent::WindowDeactivate);
    QApplication::sendEvent(&window, &deactivate);
    QVERIFY(!window.session().enabled());
  }
};

QTEST_MAIN(DriveStationTest)
#include "test_drivestation.moc"
