#pragma once

#include "robot_discovery_protocol.h"
#include <QElapsedTimer>
#include <QNetworkInterface>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>

namespace ds {
struct DiscoveryTarget {
  QHostAddress address = QHostAddress(QStringLiteral("224.0.0.251"));
  quint16 port = discovery::Port;
};

// One-shot IPv4 mDNS only. It never opens the robot's control ports.
class RobotDiscovery : public QObject {
  Q_OBJECT
public:
  static constexpr int SearchTimeoutMs = 2500;
  static constexpr int MaximumTtlSeconds = 10;
  explicit RobotDiscovery(QObject* parent = nullptr, DiscoveryTarget target = {});
  ~RobotDiscovery() override;
  bool start(int team);
  void cancel();
  bool active() const { return active_; }
  QVector<QHostAddress> addresses() const;
  QString statusText() const { return status_; }
  quint64 sentCount() const { return sentCount_; }
  quint64 rejectedCount() const { return rejectedCount_; }

signals:
  void changed();

private:
  struct Entry {
    QHostAddress address;
    qint64 expiresMs;
  };
  void sendQueries();
  void receive();
  void maintain();
  void finish(const QString& status);
  bool allowedSender(const QHostAddress& address) const;
  QUdpSocket socket_;
  QTimer retry_;
  QTimer maintenance_;
  QElapsedTimer clock_;
  DiscoveryTarget target_;
  QVector<QNetworkInterface> interfaces_;
  QVector<Entry> entries_;
  QByteArray query_;
  QString status_ = QStringLiteral("No discovery query sent.");
  int team_ = 0;
  quint16 id_ = 0;
  bool active_ = false;
  bool loopback_ = false;
  bool receiveScheduled_ = false;
  quint64 generation_ = 0;
  quint64 sentCount_ = 0;
  quint64 rejectedCount_ = 0;
};
} // namespace ds
