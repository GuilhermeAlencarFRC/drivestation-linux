#include "udp_robot_link.h"
#include <QNetworkDatagram>
#include <QRandomGenerator>

namespace ds {
UdpRobotLink::UdpRobotLink(QObject* parent) : QObject(parent), socket_(this), timer_(this) {
  timer_.setInterval(wire::SendIntervalMs);
  timer_.setTimerType(Qt::PreciseTimer);
  connect(&timer_, &QTimer::timeout, this, &UdpRobotLink::sendNow);
  connect(&socket_, &QUdpSocket::readyRead, this, &UdpRobotLink::receive);
}

UdpRobotLink::~UdpRobotLink() { stop(); }

bool UdpRobotLink::start(const QHostAddress& peer, quint16 peerPort, quint16 localPort,
                         const QHostAddress& bindAddress, bool automaticSending) {
  stop();
  lastError_.clear();
  sentCount_ = receivedCount_ = rejectedCount_ = 0;
  if (peer.protocol() != QAbstractSocket::IPv4Protocol || peer.isNull() || peer.isMulticast() ||
      peer == QHostAddress::AnyIPv4 || peer == QHostAddress::Broadcast || peerPort == 0 ||
      bindAddress.protocol() != QAbstractSocket::IPv4Protocol) {
    fail(QStringLiteral("An explicit unicast IPv4 endpoint and nonzero robot port are required."));
    return false;
  }
  if (!socket_.bind(bindAddress, localPort, QUdpSocket::DontShareAddress)) {
    fail(socket_.errorString());
    return false;
  }
  peer_ = peer;
  peerPort_ = peerPort;
  clock_.start();
  connection_.start(static_cast<quint16>(QRandomGenerator::global()->generate()), 0);
  if (automaticSending)
    timer_.start();
  sendNow();
  return connection_.active();
}

void UdpRobotLink::fail(const QString& error) {
  lastError_ = error;
  timer_.stop();
  connection_.stop();
  socket_.close();
  emit errorOccurred(lastError_);
  emit changed();
}

void UdpRobotLink::stop() {
  timer_.stop();
  if (!connection_.active())
    return;
  connection_.disable();
  // Best effort: a final disabled frame. UDP cannot guarantee delivery.
  QString ignored;
  const auto packet = wire::encodeControl(connection_.nextControl(clock_.elapsed()), ignored);
  if (packet)
    socket_.writeDatagram(*packet, peer_, peerPort_);
  connection_.stop();
  socket_.close();
  emit changed();
}

void UdpRobotLink::sendNow() {
  if (!connection_.active())
    return;
  QString error;
  const auto packet = wire::encodeControl(connection_.nextControl(clock_.elapsed()), error);
  if (!packet || socket_.writeDatagram(*packet, peer_, peerPort_) != packet->size()) {
    fail(error.isEmpty() ? socket_.errorString() : error);
    return;
  }
  ++sentCount_;
  emit changed();
}

void UdpRobotLink::receive() {
  if (!connection_.active())
    return;
  connection_.expire(clock_.elapsed());
  int budget = 64;
  while (socket_.hasPendingDatagrams() && budget-- > 0) {
    if (socket_.pendingDatagramSize() > wire::MaxDatagram) {
      socket_.receiveDatagram(0);
      ++rejectedCount_;
      continue;
    }
    const auto datagram = socket_.receiveDatagram(wire::MaxDatagram);
    if (!datagram.isValid()) {
      fail(socket_.errorString());
      return;
    }
    if (datagram.senderAddress() != peer_ || datagram.senderPort() != peerPort_) {
      ++rejectedCount_;
      continue;
    }
    QString error;
    const auto status = wire::decodeStatus(datagram.data(), error);
    if (!status || !connection_.accept(*status, clock_.elapsed())) {
      ++rejectedCount_;
      continue;
    }
    ++receivedCount_;
  }
  if (socket_.hasPendingDatagrams())
    QTimer::singleShot(0, this, &UdpRobotLink::receive);
  emit changed();
}

bool UdpRobotLink::requestEnable() {
  const bool accepted = connection_.requestEnable(clock_.isValid() ? clock_.elapsed() : 0);
  if (accepted)
    sendNow();
  return accepted;
}
void UdpRobotLink::disable() {
  connection_.disable();
  sendNow();
}
void UdpRobotLink::emergencyStop() {
  connection_.emergencyStop();
  sendNow();
}
bool UdpRobotLink::setMode(wire::Mode mode) {
  const bool valid = connection_.setMode(mode);
  sendNow();
  return valid;
}
bool UdpRobotLink::setStation(quint8 station) {
  const bool valid = connection_.setStation(station);
  sendNow();
  return valid;
}
bool UdpRobotLink::setJoysticks(const QVector<wire::Joystick>& joysticks) {
  QString error;
  if (connection_.setJoysticks(joysticks, error))
    return true;
  lastError_ = error;
  sendNow();
  emit errorOccurred(error);
  return false;
}
} // namespace ds
