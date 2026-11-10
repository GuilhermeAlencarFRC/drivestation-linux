#include "network_session.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QSaveFile>

namespace ds {
NetworkSession::NetworkSession(ControllerHub& controllers, QObject* parent)
    : QObject(parent), controllers_(controllers), link_(this), tcp_(this), timer_(this) {
  clock_.start();
  timer_.setInterval(wire::SendIntervalMs);
  timer_.setTimerType(Qt::PreciseTimer);
  connect(&timer_, &QTimer::timeout, this, [this] {
    // Sample immediately before transmission. A stalled event loop also trips
    // RobotConnection's send-gap policy before another enabled frame can leave.
    controllers_.poll();
    updateInputs();
    link_.sendNow();
  });
  connect(&controllers_, &ControllerHub::topologyChanging, this, &NetworkSession::disable);
  connect(&controllers_, &ControllerHub::updated, this, &NetworkSession::updateInputs);
  connect(&link_, &UdpRobotLink::changed, this, &NetworkSession::updateState);
  connect(&link_, &UdpRobotLink::statusReceived, this,
          [this](const wire::Status& status, qint64 elapsedMs, qint64 roundTripMs, bool reconnected) {
            if (reconnected)
              telemetry_.breakSegment();
            if (const auto sample = telemetry_.append(elapsedMs, roundTripMs, status))
              emit telemetrySampled(*sample);
          });
  connect(&link_, &UdpRobotLink::errorOccurred, this, &NetworkSession::errorOccurred);
  connect(&tcp_, &TcpRobotLink::errorOccurred, this, &NetworkSession::errorOccurred);
  connect(this, &NetworkSession::errorOccurred, this, [this](const QString& error) {
    telemetry_.event(link_.elapsedMs(), error);
  });
  connect(&tcp_, &TcpRobotLink::messageReceived, this, [this](const tcp::Message& message) {
    telemetry_.message(link_.elapsedMs(), message);
  });
  connect(&tcp_, &TcpRobotLink::messageReceived, this, &NetworkSession::messageReceived);
  connect(&tcp_, &TcpRobotLink::changed, this, [this] {
    if (!tcp_.active())
      nextTcpAttemptMs_ = clock_.elapsed() + 1000;
    if (usesTcp() && !tcp_.ready() && link_.connection().enableRequested())
      disable();
    emit changed();
  });
}

NetworkSession::~NetworkSession() { stop(); }

bool NetworkSession::start(const QHostAddress& peer, quint16 peerPort, quint16 localPort,
                           quint16 tcpPort) {
  stop();
  telemetry_.clear();
  startedUtc_ = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  peer_ = peer;
  peerPort_ = peerPort;
  tcpPort_ = tcpPort;
  nextTcpAttemptMs_ = clock_.elapsed();
  controllers_.poll();
  updateInputs();
  // The first packet already contains the current inputs and is disabled.
  const bool started =
      link_.start(peer, peerPort, localPort,
                  peer.isLoopback() ? QHostAddress::LocalHost : QHostAddress::AnyIPv4, false);
  if (started)
    timer_.start();
  telemetry_.event(link_.elapsedMs(), started ? QStringLiteral("Session started disabled.")
                                             : QStringLiteral("Session start failed."));
  return started;
}

void NetworkSession::stop() {
  timer_.stop();
  if (link_.connection().active())
    telemetry_.event(link_.elapsedMs(), QStringLiteral("Session disconnected."));
  link_.stop();
  tcp_.stop();
}

void NetworkSession::updateInputs() {
  QVector<wire::Joystick> inputs;
  QVector<tcp::JoystickDescriptor> descriptors;
  QString error;
  int index = 0;
  for (const auto& slot : controllers_.controllerSlots()) {
    wire::Joystick joystick;
    tcp::JoystickDescriptor descriptor;
    if (slot.device && slot.reading) {
      const auto& reading = *slot.reading;
      if (reading.axes.size() > wire::MaxAxes || reading.buttons.size() > wire::MaxButtons ||
          reading.povs.size() > wire::MaxPovs) {
        error = QStringLiteral("USB slot %1 exceeds UDP limits (12 axes, 32 buttons, 12 POVs).")
                    .arg(index);
        break;
      }
      for (const auto axis : reading.axes)
        joystick.axes.append(wire::quantizeAxis(axis));
      joystick.buttons = reading.buttons;
      descriptor.name = slot.device->name;
      if (usesTcp() &&
          (descriptor.name.toUtf8().size() > 255 || descriptor.name.contains(QChar(u'\0')))) {
        error = QStringLiteral("USB slot %1 has an unsupported TCP descriptor name.").arg(index);
        break;
      }
      descriptor.axisTypes.fill(0xff, reading.axes.size()); // Unknown raw SDL axis usages.
      descriptor.buttons = static_cast<quint8>(reading.buttons.size());
      descriptor.povs = static_cast<quint8>(reading.povs.size());
      for (const auto pov : reading.povs) {
        if (pov < -1 || pov > 359) {
          error = QStringLiteral("USB slot %1 has an invalid POV.").arg(index);
          break;
        }
        joystick.povs.append(static_cast<qint16>(pov));
      }
    }
    inputs.append(joystick); // Empty tags preserve every slot index.
    descriptors.append(descriptor);
    ++index;
  }
  const bool changed = inputError_ != error;
  inputError_ = error;
  link_.setJoysticks(error.isEmpty() ? inputs : QVector<wire::Joystick>{});
  if (!error.isEmpty() && link_.connection().enableRequested())
    disable();
  if (usesTcp()) {
    if (!error.isEmpty())
      descriptors = QVector<tcp::JoystickDescriptor>(wire::MaxJoysticks);
    tcp_.setStationData(descriptors, gameData_);
  }
  if (changed) {
    if (!error.isEmpty())
      emit errorOccurred(error);
    emit this->changed();
  }
}

bool NetworkSession::canEnable() const {
  const auto& connection = link_.connection();
  const auto& status = connection.status();
  return !controllers_.changingTopology() && connection.active() && status &&
         status->hasRobotCode() && !status->codeInitializing &&
         !status->enabled && !connection.enableRequested() && !connection.emergencyStopped() &&
         inputError_.isEmpty() && (!usesTcp() || tcp_.ready());
}

bool NetworkSession::enable() {
  controllers_.poll();
  updateInputs();
  const bool accepted = canEnable() && link_.requestEnable();
  if (accepted)
    telemetry_.event(link_.elapsedMs(), QStringLiteral("Enable requested explicitly."));
  return accepted;
}

void NetworkSession::disable() {
  if (link_.connection().enableRequested())
    telemetry_.event(link_.elapsedMs(), QStringLiteral("Disable requested."));
  link_.disable();
}
void NetworkSession::emergencyStop() {
  telemetry_.event(link_.elapsedMs(), QStringLiteral("E-Stop requested."));
  link_.emergencyStop();
}
void NetworkSession::setMode(wire::Mode mode) {
  if (link_.setMode(mode))
    mode_ = mode;
  emit changed();
}
void NetworkSession::setStation(quint8 station) { link_.setStation(station); }

bool NetworkSession::setGameData(const QString& data) {
  QString error;
  if (!tcp::encodeGameData(data, error)) {
    disable();
    emit errorOccurred(error);
    return false;
  }
  if (gameData_ != data) {
    disable();
    gameData_ = data;
    updateInputs();
  }
  return true;
}

bool NetworkSession::enabled() const {
  const auto& connection = link_.connection();
  return connection.status() && connection.status()->enabled;
}

qint64 NetworkSession::elapsedMs() const {
  return wasRequested_ ? clock_.elapsed() - enabledAtMs_ : elapsedMs_;
}

void NetworkSession::updateState() {
  const bool requested = link_.connection().enableRequested();
  if (requested && !wasRequested_) {
    enabledAtMs_ = clock_.elapsed();
    elapsedMs_ = 0;
  } else if (!requested && wasRequested_) {
    elapsedMs_ = clock_.elapsed() - enabledAtMs_;
  }
  wasRequested_ = requested;
  if (!link_.connection().active())
    timer_.stop();
  if (usesTcp() && link_.connection().active() && link_.connection().connected()) {
    if (!tcp_.active() && clock_.elapsed() >= nextTcpAttemptMs_) {
      nextTcpAttemptMs_ = clock_.elapsed() + 1000;
      tcp_.start(peer_, tcpPort_);
    }
  } else {
    tcp_.stop();
  }
  emit changed();
}

QString NetworkSession::statusText() const {
  const auto& connection = link_.connection();
  if (connection.emergencyStopped())
    return QStringLiteral("Emergency Stopped");
  if (!connection.active())
    return QStringLiteral("UDP Disconnected");
  if (!connection.connected())
    return QStringLiteral("No Robot Communication");
  if (!inputError_.isEmpty())
    return QStringLiteral("USB Input Unsupported");
  if (usesTcp() && !tcp_.ready())
    return tcp_.connected() ? QStringLiteral("Synchronizing TCP")
                            : QStringLiteral("Waiting for Robot TCP");
  const auto& status = *connection.status();
  if (!status.hasRobotCode())
    return QStringLiteral("No Robot Code");
  if (status.codeInitializing)
    return QStringLiteral("Robot Code Initializing");
  if (connection.enableRequested() && !status.enabled)
    return QStringLiteral("Waiting for Enable");
  if (!connection.enableRequested() && status.enabled)
    return QStringLiteral("Waiting for Disable");
  const auto name = status.mode == wire::Mode::Autonomous ? QStringLiteral("Autonomous")
                    : status.mode == wire::Mode::Test     ? QStringLiteral("Test")
                                                          : QStringLiteral("Teleoperated");
  return name + (status.enabled ? QStringLiteral(" Enabled") : QStringLiteral(" Disabled"));
}

QJsonObject NetworkSession::diagnosticReport() const {
  auto report = telemetry_.toJson();
  report.insert(QStringLiteral("format"), QStringLiteral("drivestation-linux-diagnostics"));
  report.insert(QStringLiteral("format_version"), 1);
  report.insert(QStringLiteral("app_version"), QStringLiteral(DRIVESTATION_VERSION));
  report.insert(QStringLiteral("source"), QStringLiteral("experimental-network"));
  report.insert(QStringLiteral("physical_robot_validated"), false);
  report.insert(QStringLiteral("started_utc"), startedUtc_);
  report.insert(QStringLiteral("exported_utc"),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
  report.insert(QStringLiteral("connection"),
                QJsonObject{{"peer_ipv4", peer_.toString()}, {"peer_udp_port", peerPort_},
                             {"local_udp_port", link_.localPort()}, {"tcp_port", tcpPort_},
                             {"udp_sent", qint64(link_.sentCount())},
                             {"udp_received", qint64(link_.receivedCount())},
                             {"udp_rejected", qint64(link_.rejectedCount())},
                             {"tcp_sent", qint64(tcp_.sentCount())},
                             {"tcp_received", qint64(tcp_.receivedCount())},
                             {"status", statusText()},
                             {"enable_requested", link_.connection().enableRequested()},
                             {"emergency_stopped", link_.connection().emergencyStopped()},
                             {"udp_error", link_.lastError()}, {"tcp_error", tcp_.lastError()}});
  return report;
}

bool NetworkSession::saveDiagnostics(const QString& path, QString& error) {
  disable(); // File dialogs and I/O must never preserve a pending Enable.
  error.clear();
  if (path.isEmpty()) {
    error = QStringLiteral("Choose a diagnostics file.");
    return false;
  }
  QSaveFile file(path);
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly)) {
    error = file.errorString();
    return false;
  }
  const auto data = QJsonDocument(diagnosticReport()).toJson(QJsonDocument::Indented);
  if (file.write(data) != data.size() || !file.commit()) {
    error = file.errorString();
    return false;
  }
  return true;
}
} // namespace ds
