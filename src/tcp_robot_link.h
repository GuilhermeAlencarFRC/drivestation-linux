#pragma once

#include "robot_tcp_protocol.h"
#include <QElapsedTimer>
#include <QHostAddress>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

namespace ds {
class TcpRobotLink : public QObject {
  Q_OBJECT
public:
  static constexpr int ConnectTimeoutMs = 1500;
  static constexpr int PartialFrameTimeoutMs = 1000;
  static constexpr int MaxQueuedWrite = 16384;
  static constexpr int MaxFramesPerSecond = 512;
  static constexpr int MaxBytesPerSecond = 256 * 1024;
  explicit TcpRobotLink(QObject* parent = nullptr);
  ~TcpRobotLink() override;
  bool start(const QHostAddress& peer, quint16 port);
  void stop();
  bool setStationData(const QVector<tcp::JoystickDescriptor>& descriptors, const QString& gameData);
  bool active() const { return active_; }
  bool connected() const { return socket_.state() == QAbstractSocket::ConnectedState; }
  // Means the latest local snapshot drained to the socket, not a robot acknowledgement.
  bool ready() const { return connected() && synchronized_; }
  quint64 sentCount() const { return sentCount_; }
  quint64 receivedCount() const { return receivedCount_; }
  QString lastError() const { return lastError_; }
  const QVector<tcp::Message>& versions() const { return versions_; }

signals:
  void changed();
  void errorOccurred(const QString& error);
  void messageReceived(const ds::tcp::Message& message);

private:
  void fail(const QString& error);
  void receive();
  void synchronize();
  void checkDeadlines();
  QTcpSocket socket_;
  QTimer watchdog_;
  QElapsedTimer clock_;
  tcp::StreamDecoder decoder_;
  QByteArray stationData_;
  QVector<tcp::Message> versions_;
  QString lastError_;
  bool active_ = false;
  bool synchronized_ = false;
  bool receiveScheduled_ = false;
  qint64 partialSinceMs_ = -1;
  qint64 windowSinceMs_ = 0;
  int windowFrames_ = 0;
  qint64 windowBytes_ = 0;
  quint64 sentCount_ = 0;
  quint64 receivedCount_ = 0;
};
} // namespace ds
