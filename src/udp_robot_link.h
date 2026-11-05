#pragma once

#include "robot_connection.h"
#include <QElapsedTimer>
#include <QHostAddress>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>

namespace ds {
class UdpRobotLink : public QObject {
  Q_OBJECT
public:
  explicit UdpRobotLink(QObject* parent = nullptr);
  ~UdpRobotLink() override;
  // Explicit IPv4 endpoint only. No discovery, FMS or TCP handshake in this profile.
  bool start(const QHostAddress& peer, quint16 peerPort, quint16 localPort,
             const QHostAddress& bindAddress = QHostAddress::AnyIPv4, bool automaticSending = true);
  void stop();
  void sendNow();
  bool requestEnable();
  void disable();
  void emergencyStop();
  bool setMode(wire::Mode mode);
  bool setStation(quint8 station);
  bool setJoysticks(const QVector<wire::Joystick>& joysticks);
  const RobotConnection& connection() const { return connection_; }
  quint16 localPort() const { return socket_.localPort(); }
  quint64 sentCount() const { return sentCount_; }
  quint64 receivedCount() const { return receivedCount_; }
  quint64 rejectedCount() const { return rejectedCount_; }
  QString lastError() const { return lastError_; }

signals:
  void changed();
  void errorOccurred(const QString& error);

private:
  void receive();
  void fail(const QString& error);
  QUdpSocket socket_;
  QTimer timer_;
  QElapsedTimer clock_;
  RobotConnection connection_;
  QHostAddress peer_;
  quint16 peerPort_ = 0;
  quint64 sentCount_ = 0;
  quint64 receivedCount_ = 0;
  quint64 rejectedCount_ = 0;
  QString lastError_;
};
} // namespace ds
