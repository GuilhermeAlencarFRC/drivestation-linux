#pragma once

#include "controller_hub.h"
#include "tcp_robot_link.h"
#include "telemetry_history.h"
#include "udp_robot_link.h"

namespace ds {
// Experimental robot operation, independent of DemoSession and its generated telemetry.
class NetworkSession : public QObject {
  Q_OBJECT
public:
  explicit NetworkSession(ControllerHub& controllers, QObject* parent = nullptr);
  ~NetworkSession() override;
  bool start(const QHostAddress& peer, quint16 peerPort, quint16 localPort, quint16 tcpPort = 0);
  void stop();
  bool enable();
  void disable();
  void emergencyStop();
  void setMode(wire::Mode mode);
  void setStation(quint8 station);
  bool setGameData(const QString& data);
  bool canEnable() const;
  bool enabled() const;
  qint64 elapsedMs() const;
  QString statusText() const;
  QString inputError() const { return inputError_; }
  wire::Mode mode() const { return mode_; }
  const UdpRobotLink& link() const { return link_; }
  const TcpRobotLink& tcpLink() const { return tcp_; }
  bool usesTcp() const { return tcpPort_ != 0; }
  const TelemetryHistory& telemetry() const { return telemetry_; }
  QJsonObject diagnosticReport() const;
  bool saveDiagnostics(const QString& path, QString& error);

signals:
  void changed();
  void errorOccurred(const QString& error);
  void messageReceived(const ds::tcp::Message& message);
  void telemetrySampled(const ds::TelemetrySample& sample);

private:
  void updateInputs();
  void updateState();
  ControllerHub& controllers_;
  UdpRobotLink link_;
  TcpRobotLink tcp_;
  QTimer timer_;
  QElapsedTimer clock_;
  wire::Mode mode_ = wire::Mode::Teleoperated;
  QString inputError_;
  QString gameData_;
  QHostAddress peer_;
  quint16 tcpPort_ = 0;
  qint64 nextTcpAttemptMs_ = 0;
  bool wasRequested_ = false;
  qint64 enabledAtMs_ = 0;
  qint64 elapsedMs_ = 0;
  TelemetryHistory telemetry_;
  QString startedUtc_;
  quint16 peerPort_ = 0;
};
} // namespace ds
