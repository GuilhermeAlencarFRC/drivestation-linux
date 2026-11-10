#include "discovery_test_peer.h"
#include "main_window.h"
#include "tcp_test_peer.h"
#include "widgets.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkDatagram>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QtTest>

namespace {
class InputBackend final : public ds::ControllerBackend {
public:
  QVector<ds::ControllerInfo> connected{{1, QStringLiteral("First")},
                                        {2, QStringLiteral("Second")}};
  ds::ControllerReading reading{{-32768, 0, 32767}, {true, false, true}, {-1, 315}, false};
  bool readable = true;
  QHash<ds::ControllerId, ds::ControllerReading> overrides;
  bool initialize(QString&) override { return true; }
  QVector<ds::ControllerInfo> devices(QString&) override { return connected; }
  std::optional<ds::ControllerReading> read(ds::ControllerId id, QString& error) override {
    if (readable)
      return overrides.value(id, reading);
    error = QStringLiteral("Read failed");
    return std::nullopt;
  }
  void retain(const QSet<ds::ControllerId>&) override {}
  bool rumble(ds::ControllerId, quint16, quint16, quint32, QString&) override { return false; }
};

class Peer : public QObject {
public:
  QUdpSocket socket;
  QVector<ds::wire::Control> controls;
  bool replying = true;
  bool acknowledgeEnable = true;
  bool code = true;
  quint16 batteryRaw = 0x0c80;
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
        status.mode = control->mode;
        status.enabled = control->enabled && acknowledgeEnable;
        status.emergencyStopped = control->emergencyStopped;
        status.trace = code ? 0x30 : 0x10;
        status.batteryRaw = batteryRaw; // Explicit 12.5 V by default, unlike the demo voltage.
        const auto bytes = ds::wire::encodeStatus(status, error);
        if (bytes)
          socket.writeDatagram(*bytes, datagram.senderAddress(), datagram.senderPort());
      }
    });
  }
};

template <class T> T* child(ds::MainWindow& window, const char* name) {
  return window.findChild<T*>(QString::fromLatin1(name));
}

void connectWindow(ds::MainWindow& window, Peer& peer) {
  child<QAction>(window, "robotConnection")->trigger();
  child<QCheckBox>(window, "useTcp")->setChecked(false); // Exercise the explicit UDP-only profile.
  child<QSpinBox>(window, "robotPort")->setValue(peer.socket.localPort());
  child<QSpinBox>(window, "localPort")->setValue(0);
  child<QPushButton>(window, "connectRobot")->click();
  child<QDialog>(window, "robotConnectionDialog")->hide();
}
} // namespace

class NetworkUiTest : public QObject {
  Q_OBJECT
private slots:
  void slotSwapDisablesBeforeSendingNewInputsAndDescriptors() {
    Peer udp;
    ds::test::TcpPeer tcp;
    auto backend = std::make_unique<InputBackend>();
    backend->overrides.insert(1, {{-32768}, {}, {}, false});
    backend->overrides.insert(2, {{32767}, {}, {}, false});
    ds::ControllerHub controllers(std::move(backend));
    controllers.start();
    ds::NetworkSession session(controllers);
    QVERIFY(session.start(QHostAddress::LocalHost, udp.socket.localPort(), 0,
                           tcp.server.serverPort()));
    QTRY_VERIFY(session.canEnable());
    QVERIFY(session.enable());
    QTRY_VERIFY(session.enabled());
    QCOMPARE(udp.controls.last().joysticks[0].axes[0], qint8(-128));
    connect(&controllers, &ds::ControllerHub::topologyChanging, &session, [&] {
      QVERIFY(!session.enable()); // Reentrant requests cannot re-enable the old mapping.
    });
    const auto before = udp.controls.size();
    QVERIFY(controllers.swapSlots(0, 1));
    QVERIFY(!session.link().connection().enableRequested());
    QTRY_COMPARE(udp.controls.last().joysticks[0].axes[0], qint8(127));
    for (qsizetype i = before; i < udp.controls.size(); ++i)
      if (udp.controls[i].joysticks[0].axes[0] == 127)
        QVERIFY(!udp.controls[i].enabled);
    QTRY_VERIFY(tcp.frames.size() >= 14);
    QCOMPARE(tcp.frames[tcp.frames.size() - 7].payload.mid(4, 6), QByteArray("Second"));
    QVERIFY(!session.link().connection().enableRequested());
    QTRY_VERIFY(session.canEnable());
    QVERIFY(session.enable());
    QTRY_VERIFY(session.enabled());
  }

  void receivedChartsLeaveGapsAndKeepDemoSeparate() {
    QTemporaryDir directory;
    Peer peer;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath("ui.ini")), nullptr,
                          std::make_unique<InputBackend>());
    window.show();
    auto* chart = child<ds::TelemetryChart>(window, "telemetryChart");
    child<QCheckBox>(window, "simulateRobot")->setChecked(true);
    QTRY_VERIFY(!chart->samples().isEmpty());
    QCOMPARE(chart->source(), ds::TelemetryChart::Source::Simulation);
    connectWindow(window, peer);
    QCOMPARE(chart->source(), ds::TelemetryChart::Source::Network);
    QTRY_VERIFY(chart->samples().size() >= 3);
    for (const auto& sample : chart->samples())
      QCOMPARE(sample.voltage, 12.5);
    window.selectRightView(1);
    const auto evidence = qEnvironmentVariable("DS_EVIDENCE_DIR");
    if (!evidence.isEmpty())
      QVERIFY(window.grab().save(QDir(evidence).filePath("network-charts.png")));
    peer.replying = false;
    QTRY_VERIFY(!window.network().link().connection().connected());
    const auto count = chart->samples().size();
    peer.socket.writeDatagram(QByteArray("invalid"), QHostAddress::LocalHost,
                              window.network().link().localPort());
    QTRY_VERIFY(window.network().link().rejectedCount() > 0);
    QTest::qWait(300);
    QCOMPARE(chart->samples().size(), count);
    peer.replying = true;
    QTRY_VERIFY(chart->samples().size() > count);
    QVERIFY(chart->samples()[count].startsSegment);
    window.network().stop();
    child<QCheckBox>(window, "simulateRobot")->setChecked(true);
    QCOMPARE(chart->source(), ds::TelemetryChart::Source::Simulation);
    QVERIFY(chart->samples().isEmpty());
  }

  void exportedDiagnosticsDisableAndResetBetweenSessions() {
    QTemporaryDir directory;
    Peer peer;
    auto backend = std::make_unique<InputBackend>();
    ds::ControllerHub controllers(std::move(backend));
    controllers.start();
    ds::NetworkSession session(controllers);
    QVERIFY(session.start(QHostAddress::LocalHost, peer.socket.localPort(), 0));
    QTRY_VERIFY(session.canEnable());
    QVERIFY(session.enable());
    QTRY_VERIFY(session.enabled());
    const auto path = directory.filePath(QStringLiteral("diagnóstico recebido.json"));
    QString error;
    QVERIFY2(session.saveDiagnostics(path, error), qPrintable(error));
    QVERIFY(!session.link().connection().enableRequested());
    QTRY_VERIFY(!peer.controls.last().enabled);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto exported = file.readAll();
    const auto report = QJsonDocument::fromJson(exported).object();
    QCOMPARE(report.value("format").toString(), QStringLiteral("drivestation-linux-diagnostics"));
    QCOMPARE(report.value("app_version").toString(), QStringLiteral(DRIVESTATION_VERSION));
    QCOMPARE(report.value("source").toString(), QStringLiteral("experimental-network"));
    QVERIFY(!report.value("samples").toArray().isEmpty());
    QCOMPARE(report.value("samples").toArray().first().toObject()
                 .value("battery_volts").toDouble(), 12.5);
    QVERIFY(!report.value("connection").toObject().value("enable_requested").toBool());
    Peer other;
    other.batteryRaw = 0x0b00;
    QVERIFY(session.start(QHostAddress::LocalHost, other.socket.localPort(), 0));
    QVERIFY(session.telemetry().samples().empty());
    QTRY_VERIFY(!session.telemetry().samples().empty());
    QCOMPARE(session.telemetry().samples().front().status.voltage(), 11.0);
    for (const auto& message : session.diagnosticReport().value("messages").toArray())
      QVERIFY(!message.toObject().value("text").toString().contains(QStringLiteral("Enable requested")));
    QVERIFY(session.canEnable());
    QVERIFY(session.enable());
    QVERIFY(!session.saveDiagnostics(path + QStringLiteral("/child.json"), error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!session.link().connection().enableRequested());
    QVERIFY(!session.saveDiagnostics({}, error));
    const auto evidence = qEnvironmentVariable("DS_EVIDENCE_DIR");
    if (!evidence.isEmpty()) {
      QFile example(QDir(evidence).filePath("diagnostics-example.json"));
      QVERIFY(example.open(QIODevice::WriteOnly));
      QCOMPARE(example.write(exported), qint64(exported.size()));
    }
  }

  void cancellingExportDialogStillDisablesOperation() {
    QTemporaryDir directory;
    Peer peer;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath("ui.ini")), nullptr,
                          std::make_unique<InputBackend>());
    window.show();
    auto* action = child<QAction>(window, "exportNetworkDiagnostics");
    QVERIFY(!action->isEnabled());
    connectWindow(window, peer);
    QTRY_VERIFY(window.network().canEnable());
    child<QPushButton>(window, "enable")->click();
    QTRY_VERIFY(window.network().enabled());
    bool sawDialog = false;
    bool disabledBeforeDialog = false;
    QTimer closer;
    connect(&closer, &QTimer::timeout, &window, [&] {
      auto* dialog = child<QFileDialog>(window, "exportDiagnosticsDialog");
      sawDialog = dialog;
      disabledBeforeDialog = !window.network().link().connection().enableRequested();
      if (dialog)
        dialog->reject();
    });
    closer.start(10);
    action->trigger();
    closer.stop();
    QVERIFY(sawDialog);
    QVERIFY(disabledBeforeDialog);
    QVERIFY(!window.network().link().connection().enableRequested());
    QTRY_VERIFY(!peer.controls.last().enabled);
  }

  void discoverySelectionDoesNotConnectOrEnableRobot() {
    QTemporaryDir directory;
    Peer udp;
    ds::test::DiscoveryPeer dns;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath("ui.ini")), nullptr,
                          std::make_unique<InputBackend>(),
                          {QHostAddress::LocalHost, dns.socket.localPort()});
    window.show();
    child<QAction>(window, "robotConnection")->trigger();
    QCOMPARE(window.discovery().sentCount(), quint64(0));
    QVERIFY(!child<QPushButton>(window, "findRobot")->isEnabled());
    child<QSpinBox>(window, "discoveryTeam")->setValue(10327);
    child<QLineEdit>(window, "robotAddress")->setText(QStringLiteral("192.0.2.77"));
    child<QSpinBox>(window, "robotPort")->setValue(udp.socket.localPort());
    child<QSpinBox>(window, "localPort")->setValue(0);
    child<QCheckBox>(window, "useTcp")->setChecked(false);
    auto* list = child<QListWidget>(window, "discoveryResults");
    QCOMPARE(list->count(), 2); // Unverified USB and TE.AM hints, no automatic selection.
    QVERIFY(!child<QPushButton>(window, "useDiscoveryAddress")->isEnabled());
    child<QPushButton>(window, "findRobot")->click();
    QTRY_COMPARE(window.discovery().addresses().size(), 1);
    QTRY_COMPARE(list->count(), 3);
    QCOMPARE(child<QLineEdit>(window, "robotAddress")->text(), QStringLiteral("192.0.2.77"));
    QCOMPARE(window.network().link().sentCount(), quint64(0));
    QVERIFY(udp.controls.isEmpty());
    QVERIFY(!window.network().link().connection().active());
    QVERIFY(!window.session().enabled());
    QVERIFY(!child<QPushButton>(window, "useDiscoveryAddress")->isEnabled());
    list->setCurrentRow(0);
    const auto evidence = qEnvironmentVariable("DS_EVIDENCE_DIR");
    if (!evidence.isEmpty())
      QVERIFY(child<QDialog>(window, "robotConnectionDialog")
                  ->grab()
                  .save(QDir(evidence).filePath("discovery-results.png")));
    child<QPushButton>(window, "useDiscoveryAddress")->click();
    QCOMPARE(child<QLineEdit>(window, "robotAddress")->text(), QStringLiteral("127.0.0.1"));
    QVERIFY(!window.discovery().active());
    QCOMPARE(window.network().link().sentCount(), quint64(0));
    child<QPushButton>(window, "connectRobot")->click();
    QTRY_VERIFY(window.network().canEnable());
    QVERIFY(!window.network().link().connection().enableRequested());
    QVERIFY(!child<QPushButton>(window, "findRobot")->isEnabled());
    QVERIFY(!child<QPushButton>(window, "useDiscoveryAddress")->isEnabled());
    QVERIFY(!child<QSpinBox>(window, "discoveryTeam")->isEnabled());
    child<QDialog>(window, "robotConnectionDialog")->hide();
    child<QPushButton>(window, "enable")->click();
    QTRY_VERIFY(window.network().enabled());
  }

  void expiredDiscoveryResultCannotBeAppliedEvenBeforeTimerRefresh() {
    QTemporaryDir directory;
    ds::test::DiscoveryPeer dns;
    dns.ttl = 1;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath("ui.ini")), nullptr,
                          std::make_unique<InputBackend>(),
                          {QHostAddress::LocalHost, dns.socket.localPort()});
    child<QAction>(window, "robotConnection")->trigger();
    child<QSpinBox>(window, "discoveryTeam")->setValue(1);
    child<QLineEdit>(window, "robotAddress")->setText(QStringLiteral("192.0.2.77"));
    child<QPushButton>(window, "findRobot")->click();
    QTRY_COMPARE(window.discovery().addresses().size(), 1);
    auto* list = child<QListWidget>(window, "discoveryResults");
    list->setCurrentRow(0);
    dns.replying = false;
    QTest::qSleep(1100); // Do not dispatch the maintenance timer before the Use click.
    child<QPushButton>(window, "useDiscoveryAddress")->click();
    QCOMPARE(child<QLineEdit>(window, "robotAddress")->text(), QStringLiteral("192.0.2.77"));
    QVERIFY(!child<QPushButton>(window, "useDiscoveryAddress")->isEnabled());
    QCOMPARE(window.network().link().sentCount(), quint64(0));
    child<QPushButton>(window, "cancelDiscovery")->click();
  }

  void discoveryCancelsOnChangesAndClose_data() {
    QTest::addColumn<QString>("trigger");
    for (const auto* name : {"address", "team", "hide", "close", "connect", "demo"})
      QTest::newRow(name) << QString::fromLatin1(name);
  }
  void discoveryCancelsOnChangesAndClose() {
    QFETCH(QString, trigger);
    QTemporaryDir directory;
    Peer udp;
    ds::test::DiscoveryPeer dns;
    dns.replying = false;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath("ui.ini")), nullptr,
                          std::make_unique<InputBackend>(),
                          {QHostAddress::LocalHost, dns.socket.localPort()});
    window.show();
    child<QAction>(window, "robotConnection")->trigger();
    child<QSpinBox>(window, "discoveryTeam")->setValue(1);
    child<QPushButton>(window, "findRobot")->click();
    QTRY_COMPARE(dns.queries.size(), 1);
    QVERIFY(window.discovery().active());
    if (trigger == "address")
      child<QLineEdit>(window, "robotAddress")->setText(QStringLiteral("127.0.0.2"));
    if (trigger == "team")
      child<QSpinBox>(window, "discoveryTeam")->setValue(2);
    if (trigger == "hide")
      child<QDialog>(window, "robotConnectionDialog")->hide();
    if (trigger == "close")
      window.close();
    if (trigger == "demo")
      child<QCheckBox>(window, "simulateRobot")->setChecked(true);
    if (trigger == "connect") {
      child<QSpinBox>(window, "robotPort")->setValue(udp.socket.localPort());
      child<QSpinBox>(window, "localPort")->setValue(0);
      child<QCheckBox>(window, "useTcp")->setChecked(false);
      child<QPushButton>(window, "connectRobot")->click();
    }
    QVERIFY(!window.discovery().active());
    const auto count = dns.queries.size();
    QTest::qWait(1100);
    QCOMPARE(dns.queries.size(), count);
    QVERIFY(!window.network().link().connection().enableRequested());
  }

  void tcpLossDescriptorChangesAndGameDataRequireExplicitEnable() {
    Peer udp;
    ds::test::TcpPeer tcp;
    auto backend = std::make_unique<InputBackend>();
    auto* input = backend.get();
    ds::ControllerHub hub(std::move(backend));
    hub.start(false);
    ds::NetworkSession session(hub);
    QVERIFY(session.setGameData(QStringLiteral("ABC")));
    QVERIFY(
        session.start(QHostAddress::LocalHost, udp.socket.localPort(), 0, tcp.server.serverPort()));
    QTRY_VERIFY(session.canEnable());
    QTRY_COMPARE(tcp.frames.size(), 7);
    QCOMPARE(tcp.frames.last().payload, QByteArray("ABC"));
    QVERIFY(session.enable());
    QTRY_VERIFY(session.enabled());
    tcp.socket->abort();
    QTRY_VERIFY(!session.link().connection().enableRequested());
    QVERIFY(session.link().connection().connected()); // TCP loss cancels even with fresh UDP.
    QTRY_VERIFY_WITH_TIMEOUT(session.canEnable(), 2500);
    QCOMPARE(tcp.connections, 2);
    QVERIFY(!session.enabled());
    QVERIFY(!session.link().connection().enableRequested());
    QVERIFY(session.enable());
    QTRY_VERIFY(session.enabled());
    const auto snapshots = tcp.frames.size();
    input->reading.axes.append(1234); // Counts change without an SDL instance-ID change.
    hub.poll();
    QVERIFY(!session.link().connection().enableRequested());
    QTRY_VERIFY(session.canEnable());
    QTRY_COMPARE(tcp.frames.size(), snapshots + 7);
    QVERIFY(session.enable());
    QTRY_VERIFY(session.enabled());
    QVERIFY(session.setGameData(QStringLiteral("NEW")));
    QVERIFY(!session.link().connection().enableRequested());
    QTRY_VERIFY(session.canEnable());
    QTRY_COMPARE(tcp.frames.last().payload, QByteArray("NEW"));
    QVERIFY(!session.enabled());
    QVERIFY(session.enable());
    QTRY_VERIFY(session.enabled());
    tcp.send(QByteArray::fromHex("ffff"));
    QTRY_VERIFY(!session.link().connection().enableRequested());
    QVERIFY(!session.canEnable());
    QTRY_VERIFY_WITH_TIMEOUT(session.canEnable(), 2500);
    QVERIFY(!session.enabled());
  }

  void udpAvailabilityAloneDoesNotEnableTheTcpProfile() {
    Peer udp;
    ds::test::TcpPeer tcp;
    const auto port = tcp.server.serverPort();
    tcp.server.close();
    ds::ControllerHub hub(std::make_unique<InputBackend>());
    hub.start(false);
    ds::NetworkSession session(hub);
    QVERIFY(session.start(QHostAddress::LocalHost, udp.socket.localPort(), 0, port));
    QTRY_VERIFY(session.link().connection().connected());
    QVERIFY(!session.canEnable());
    QVERIFY(!session.enable());
    QTRY_VERIFY(!session.tcpLink().lastError().isEmpty());
    QVERIFY(!session.link().connection().enableRequested());
    session.stop();
    const auto connections = tcp.connections;
    QVERIFY(tcp.server.listen(QHostAddress::LocalHost, port));
    QTest::qWait(1100); // Stop must cancel retry scheduling as well as packet transmission.
    QCOMPARE(tcp.connections, connections);
    QVERIFY(!session.tcpLink().active());
  }

  void uiReceivesTcpConsoleAndVersionsWithoutMixingDemoData() {
    QTemporaryDir directory;
    Peer udp;
    ds::test::TcpPeer tcp;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath("ui.ini")), nullptr,
                          std::make_unique<InputBackend>());
    window.show();
    child<QLineEdit>(window, "gameData")->setText(QStringLiteral("GAME"));
    child<QAction>(window, "robotConnection")->trigger();
    QVERIFY(child<QCheckBox>(window, "useTcp")->isChecked());
    child<QSpinBox>(window, "robotPort")->setValue(udp.socket.localPort());
    child<QSpinBox>(window, "tcpPort")->setValue(tcp.server.serverPort());
    child<QSpinBox>(window, "localPort")->setValue(0);
    child<QPushButton>(window, "connectRobot")->click();
    child<QDialog>(window, "robotConnectionDialog")->hide();
    QTRY_VERIFY(window.network().canEnable());
    QTRY_COMPARE(tcp.frames.size(), 7);
    QCOMPARE(tcp.frames.last().payload, QByteArray("GAME"));
    const auto send = [&](quint8 id, const QByteArray& payload) {
      QString error;
      tcp.send(ds::tcp::encodeFrame(id, payload, error).value());
    };
    auto stdoutPayload = QByteArray::fromHex("3f800000002a");
    stdoutPayload.append("<b>Robot says hello</b>");
    send(0x0c, stdoutPayload);
    auto version = QByteArray::fromHex("000000000d");
    version.append("roboRIO image");
    version.append(char(6));
    version.append("2026.1");
    send(0x0a, version);
    auto* console = child<QPlainTextEdit>(window, "messages");
    QTRY_VERIFY(console->toPlainText().contains(QStringLiteral("<b>Robot says hello</b>")));
    QTRY_VERIFY(child<QLabel>(window, "diagnostics")->text().contains(QStringLiteral("2026.1")));
    bool recordedStdout = false;
    for (const auto& message : window.network().diagnosticReport().value("messages").toArray())
      recordedStdout |= message.toObject().value("kind").toString() == QStringLiteral("stdout") &&
                        message.toObject().value("text").toString() ==
                            QStringLiteral("<b>Robot says hello</b>");
    QVERIFY(recordedStdout);
    QCOMPARE(child<QLabel>(window, "dataSource")->text(),
             QStringLiteral("ALFA · TESTES · UDP/TCP"));
    child<QPushButton>(window, "enable")->click();
    QTRY_VERIFY(window.network().enabled());
    const auto evidence = qEnvironmentVariable("DS_EVIDENCE_DIR");
    if (!evidence.isEmpty()) {
      QVERIFY(window.grab().save(QDir(evidence).filePath("tcp-console.png")));
      window.selectTab(1);
      QVERIFY(window.grab().save(QDir(evidence).filePath("tcp-diagnostics.png")));
      child<QAction>(window, "robotConnection")->trigger();
      QTRY_VERIFY(!window.network().enabled());
      QVERIFY(child<QDialog>(window, "robotConnectionDialog")
                  ->grab()
                  .save(QDir(evidence).filePath("tcp-connection.png")));
      child<QDialog>(window, "robotConnectionDialog")->hide();
    }
    tcp.socket->abort();
    QTRY_VERIFY(!window.network().link().connection().enableRequested());
    QTRY_VERIFY(!child<QLabel>(window, "diagnostics")->text().contains(QStringLiteral("2026.1")));
    window.close();
    QVERIFY(!window.network().tcpLink().active());
  }

  void slotsAndInputsReachUdpWithoutTruncationOrReordering() {
    Peer peer;
    QVERIFY(peer.socket.localPort());
    auto backend = std::make_unique<InputBackend>();
    auto* input = backend.get();
    ds::ControllerHub hub(std::move(backend));
    hub.start(false);
    input->connected.removeFirst(); // Slot 0 stays empty; device 2 remains in slot 1.
    hub.poll();
    ds::NetworkSession session(hub);
    QVERIFY(session.start(QHostAddress::LocalHost, peer.socket.localPort(), 0));
    QTRY_VERIFY(session.canEnable());
    QVERIFY(!peer.controls.isEmpty());
    const auto& first = peer.controls.first();
    QVERIFY(!first.enabled);
    QCOMPARE(first.joysticks.size(), 6);
    QVERIFY(first.joysticks[0].axes.isEmpty());
    QCOMPARE(first.joysticks[1].axes, QVector<qint8>({-128, 0, 127}));
    QCOMPARE(first.joysticks[1].buttons, QVector<bool>({true, false, true}));
    QCOMPARE(first.joysticks[1].povs, QVector<qint16>({-1, 315}));
    QVERIFY(session.enable());
    QTRY_VERIFY(session.enabled());
    input->reading.axes[0] = 16384;
    QTRY_COMPARE(peer.controls.last().joysticks[1].axes[0], qint8(64));
    QVERIFY(session.elapsedMs() > 0);
    input->readable = false;
    hub.poll();
    QVERIFY(!session.link().connection().enableRequested());
    QTRY_VERIFY(peer.controls.last().joysticks[1].axes.isEmpty());
    input->readable = true;
    hub.poll();
    QTRY_VERIFY(session.canEnable());
    QVERIFY(!session.link().connection().enableRequested());
    QVERIFY(!session.enabled());
  }

  void unsupportedInputsDisableUntilCorrected_data() {
    QTest::addColumn<int>("kind");
    QTest::newRow("axes") << 0;
    QTest::newRow("buttons") << 1;
    QTest::newRow("povs") << 2;
    QTest::newRow("pov-value") << 3;
  }
  void unsupportedInputsDisableUntilCorrected() {
    QFETCH(int, kind);
    Peer peer;
    auto backend = std::make_unique<InputBackend>();
    auto* input = backend.get();
    ds::ControllerHub hub(std::move(backend));
    hub.start(false);
    ds::NetworkSession session(hub);
    QVERIFY(session.start(QHostAddress::LocalHost, peer.socket.localPort(), 0));
    QTRY_VERIFY(session.canEnable());
    QVERIFY(session.enable());
    QTRY_VERIFY(session.enabled());
    const auto valid = input->reading;
    if (kind == 0)
      input->reading.axes.fill(0, 13);
    if (kind == 1)
      input->reading.buttons.fill(false, 33);
    if (kind == 2)
      input->reading.povs.fill(-1, 13);
    if (kind == 3)
      input->reading.povs = {65536}; // Cannot wrap into an apparently valid int16 POV.
    hub.poll();
    QVERIFY(!session.inputError().isEmpty());
    QVERIFY(!session.link().connection().enableRequested());
    QVERIFY(!session.enable());
    QTRY_VERIFY(!peer.controls.last().enabled && peer.controls.last().joysticks.isEmpty());
    input->reading = valid;
    hub.poll();
    QTRY_VERIFY(session.canEnable());
    QVERIFY(!session.link().connection().enableRequested());
  }

  void timeoutCodeLossAndEventLoopDelayRequireNewEnable() {
    Peer peer;
    ds::ControllerHub hub(std::make_unique<InputBackend>());
    hub.start(false);
    ds::NetworkSession session(hub);
    QVERIFY(session.start(QHostAddress::LocalHost, peer.socket.localPort(), 0));
    QTRY_VERIFY(session.canEnable());
    QVERIFY(session.enable());
    QTRY_VERIFY(session.enabled());
    peer.replying = false;
    QTRY_VERIFY(!session.link().connection().connected());
    QVERIFY(!session.link().connection().enableRequested());
    QCOMPARE(session.statusText(), QStringLiteral("No Robot Communication"));
    peer.replying = true;
    QTRY_VERIFY(session.canEnable());
    QVERIFY(!session.enabled());
    QVERIFY(session.enable());
    QTRY_VERIFY(session.enabled());
    peer.code = false;
    QTRY_VERIFY(!session.link().connection().enableRequested());
    QVERIFY(!session.canEnable());
    peer.code = true;
    QTRY_VERIFY(session.canEnable());
    QVERIFY(session.enable());
    QTRY_VERIFY(session.enabled());
    // Intentionally block both timers; the next transmission must be disabled.
    QTest::qSleep(ds::RobotConnection::MaxSendGapMs + 20);
    QTRY_VERIFY(!session.link().connection().enableRequested());
    QTRY_VERIFY(session.canEnable());
    QVERIFY(!session.enabled());
  }

  void guiUsesReceivedTelemetryAndKeepsUdpEstopLatched() {
    QTemporaryDir directory;
    Peer peer;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath("ui.ini")), nullptr,
                          std::make_unique<InputBackend>());
    window.show();
    QCOMPARE(window.network().link().sentCount(), quint64(0));
    auto* simulate = child<QCheckBox>(window, "simulateRobot");
    simulate->setChecked(true);
    QVERIFY(window.session().enable());
    connectWindow(window, peer);
    QTRY_VERIFY(window.network().canEnable());
    QVERIFY(!window.session().connected());
    QVERIFY(!simulate->isChecked());
    QVERIFY(!simulate->isEnabled());
    QVERIFY(!child<QPushButton>(window, "mode2")->isEnabled());
    QCOMPARE(child<QLabel>(window, "voltage")->text(), QStringLiteral("12.50 V"));
    QCOMPARE(child<QLabel>(window, "dataSource")->text(), QStringLiteral("ALFA · TESTES · UDP"));
    auto* enable = child<QPushButton>(window, "enable");
    peer.acknowledgeEnable = false;
    enable->click();
    QVERIFY(window.network().link().connection().enableRequested());
    QCOMPARE(child<QLabel>(window, "robotStatus")->text(), QStringLiteral("Waiting for Enable"));
    QVERIFY(!enable->isChecked());
    peer.acknowledgeEnable = true;
    QTRY_VERIFY(enable->isChecked());
    const auto evidence = qEnvironmentVariable("DS_EVIDENCE_DIR");
    if (!evidence.isEmpty()) {
      QVERIFY(window.grab().save(QDir(evidence).filePath("udp-operation.png")));
      child<QAction>(window, "robotConnection")->trigger();
      QTRY_VERIFY(!window.network().link().connection().enableRequested());
      QVERIFY(child<QDialog>(window, "robotConnectionDialog")
                  ->grab()
                  .save(QDir(evidence).filePath("udp-connection.png")));
      child<QDialog>(window, "robotConnectionDialog")->hide();
    }
    QTest::keyClick(&window, Qt::Key_Return);
    QTRY_VERIFY(!window.network().enabled());
    QVERIFY(!window.network().link().connection().enableRequested());
    QTRY_VERIFY(enable->isEnabled());
    enable->click();
    QTRY_VERIFY(window.network().enabled());
    QTest::keyClick(&window, Qt::Key_Space);
    QVERIFY(window.network().link().connection().emergencyStopped());
    QVERIFY(!enable->isEnabled());
    child<QPushButton>(window, "disconnectRobot")->click();
    QTRY_VERIFY(!peer.controls.last().enabled);
    simulate->setChecked(true);
    child<QPushButton>(window, "resetDemo")->click();
    connectWindow(window, peer);
    QTRY_VERIFY(window.network().link().connection().connected());
    QVERIFY(window.network().link().connection().emergencyStopped());
    QVERIFY(!window.network().canEnable());
    QVERIFY(!child<QPushButton>(window, "resetDemo")->isEnabled());
  }

  void guiCancelsNetworkOperation_data() {
    QTest::addColumn<QString>("trigger");
    for (const auto* trigger : {"focus", "application", "hide", "mode", "team", "station", "timing",
                                "connection-dialog", "close"})
      QTest::newRow(trigger) << QString::fromLatin1(trigger);
  }
  void guiCancelsNetworkOperation() {
    QFETCH(QString, trigger);
    QTemporaryDir directory;
    Peer peer;
    ds::MainWindow window(ds::PreferenceStore(directory.filePath("ui.ini")), nullptr,
                          std::make_unique<InputBackend>());
    window.show();
    connectWindow(window, peer);
    QTRY_VERIFY(window.network().canEnable());
    child<QPushButton>(window, "enable")->click();
    QTRY_VERIFY(window.network().enabled());
    if (trigger == "focus") {
      QEvent event(QEvent::WindowDeactivate);
      QApplication::sendEvent(&window, &event);
    } else if (trigger == "application") {
      QEvent event(QEvent::ApplicationDeactivate);
      QApplication::sendEvent(qApp, &event);
    } else if (trigger == "hide") {
      window.hide();
    } else if (trigger == "mode") {
      child<QPushButton>(window, "mode1")->click();
      QCOMPARE(window.network().mode(), ds::wire::Mode::Autonomous);
    } else if (trigger == "team") {
      child<QSpinBox>(window, "teamNumber")->setValue(10327);
    } else if (trigger == "station") {
      child<QComboBox>(window, "station")->setCurrentIndex(5);
    } else if (trigger == "timing") {
      child<QSpinBox>(window, "practice0")->setValue(2);
    } else if (trigger == "connection-dialog") {
      child<QAction>(window, "robotConnection")->trigger();
    } else if (trigger == "close") {
      window.close();
      QVERIFY(!window.network().link().connection().active());
    }
    QVERIFY(!window.network().link().connection().enableRequested());
    QTRY_VERIFY(!peer.controls.last().enabled);
    if (trigger == "station")
      QTRY_COMPARE(peer.controls.last().station, quint8(5));
    if (trigger == "mode")
      QTRY_COMPARE(peer.controls.last().mode, ds::wire::Mode::Autonomous);
  }

  void guiRejectsInvalidEndpointAndDoesNotPersistConnection() {
    QTemporaryDir directory;
    const ds::PreferenceStore store(directory.filePath("ui.ini"));
    Peer peer;
    {
      ds::MainWindow window(store, nullptr, std::make_unique<InputBackend>());
      child<QAction>(window, "robotConnection")->trigger();
      child<QLineEdit>(window, "robotAddress")->setText(QStringLiteral("224.0.0.1"));
      child<QPushButton>(window, "connectRobot")->click();
      QVERIFY(!window.network().link().connection().active());
      QVERIFY(!window.network().link().lastError().isEmpty());
      QVERIFY(!child<QLabel>(window, "connectionStatus")->text().isEmpty());
      child<QLineEdit>(window, "robotAddress")->setText(QStringLiteral("127.0.0.1"));
      connectWindow(window, peer);
      QTRY_VERIFY(window.network().canEnable());
      child<QPushButton>(window, "enable")->click();
      QTRY_VERIFY(window.network().enabled());
    } // Destructor transmits the final disabled frame.
    QTRY_VERIFY(!peer.controls.last().enabled);
    ds::MainWindow reopened(store, nullptr, std::make_unique<InputBackend>());
    QVERIFY(!reopened.network().link().connection().active());
    QVERIFY(!reopened.session().connected());
    QCOMPARE(reopened.network().link().sentCount(), quint64(0));
  }
};

QTEST_MAIN(NetworkUiTest)
#include "test_network_ui.moc"
