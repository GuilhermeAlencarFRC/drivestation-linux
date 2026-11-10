#include "robot_discovery.h"
#include <QNetworkDatagram>
#include <QRandomGenerator>
#include <QVariant>
#include <algorithm>
#include <utility>

namespace ds {
RobotDiscovery::RobotDiscovery(QObject* parent, DiscoveryTarget target)
    : QObject(parent), socket_(this), retry_(this), maintenance_(this), target_(std::move(target)) {
  retry_.setSingleShot(true);
  retry_.setInterval(1000);
  retry_.setTimerType(Qt::PreciseTimer);
  maintenance_.setInterval(100);
  connect(&retry_, &QTimer::timeout, this, &RobotDiscovery::sendQueries);
  connect(&maintenance_, &QTimer::timeout, this, &RobotDiscovery::maintain);
  connect(&socket_, &QUdpSocket::readyRead, this, &RobotDiscovery::receive);
}

RobotDiscovery::~RobotDiscovery() {
  active_ = false;
  retry_.stop();
  maintenance_.stop();
  socket_.close();
}

void RobotDiscovery::cancel() {
  ++generation_;
  active_ = false;
  receiveScheduled_ = false;
  retry_.stop();
  maintenance_.stop();
  socket_.close();
  entries_.clear();
  interfaces_.clear();
  status_ = QStringLiteral("Discovery stopped. Connect remains explicit.");
  emit changed();
}

bool RobotDiscovery::start(int team) {
  cancel();
  sentCount_ = rejectedCount_ = 0;
  QString error;
  id_ = static_cast<quint16>(QRandomGenerator::global()->bounded(1, 65536));
  const auto query = discovery::query(team, id_, error);
  if (!query) {
    status_ = error;
    emit changed();
    return false;
  }
  loopback_ = target_.address.isLoopback() && discovery::usableIpv4(target_.address);
  if (target_.port == 0 ||
      (!loopback_ && (target_.address != QHostAddress(QStringLiteral("224.0.0.251")) ||
                      target_.port != discovery::Port))) {
    status_ = QStringLiteral("Unsupported discovery transport target.");
    emit changed();
    return false;
  }
  if (!loopback_) {
    for (const auto& interface : QNetworkInterface::allInterfaces()) {
      const auto flags = interface.flags();
      if (!flags.testFlag(QNetworkInterface::IsUp) ||
          !flags.testFlag(QNetworkInterface::IsRunning) ||
          !flags.testFlag(QNetworkInterface::CanMulticast) ||
          flags.testFlag(QNetworkInterface::IsLoopBack))
        continue;
      const auto addresses = interface.addressEntries();
      if (std::any_of(addresses.begin(), addresses.end(), [](const auto& entry) {
            return discovery::usableIpv4(entry.ip()) && entry.prefixLength() > 0;
          }))
        interfaces_.append(interface);
    }
    if (interfaces_.isEmpty()) {
      status_ =
          QStringLiteral("No active IPv4 multicast interface. Manual addresses remain available.");
      emit changed();
      return false;
    }
  }
  if (!socket_.bind(loopback_ ? QHostAddress::LocalHost : QHostAddress::AnyIPv4, 0,
                    QUdpSocket::DontShareAddress)) {
    status_ = socket_.errorString();
    emit changed();
    return false;
  }
  if (socket_.localPort() == discovery::Port) {
    finish(QStringLiteral("An ephemeral port other than 5353 is required for one-shot discovery."));
    return false;
  }
  if (!loopback_) {
    socket_.setSocketOption(QAbstractSocket::MulticastTtlOption, 255);
    socket_.setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
  }
  team_ = team;
  query_ = *query;
  active_ = true;
  clock_.start();
  maintenance_.start();
  status_ = QStringLiteral("Looking up %1… No control commands sent.")
                .arg(discovery::robotHostname(team));
  sendQueries();
  if (active_)
    retry_.start();
  emit changed();
  return active_;
}

void RobotDiscovery::sendQueries() {
  if (!active_)
    return;
  if (clock_.elapsed() >= SearchTimeoutMs) {
    maintain();
    return;
  }
  const auto send = [&] {
    if (socket_.writeDatagram(query_, target_.address, target_.port) == query_.size())
      ++sentCount_;
  };
  if (loopback_)
    send();
  else
    for (const auto& interface : interfaces_) {
      socket_.setMulticastInterface(interface);
      send();
    }
  if (sentCount_ == 0)
    finish(QStringLiteral("Could not send discovery query: %1").arg(socket_.errorString()));
}

bool RobotDiscovery::allowedSender(const QHostAddress& address) const {
  if (!discovery::usableIpv4(address))
    return false;
  if (loopback_)
    return address == target_.address;
  for (const auto& interface : interfaces_)
    for (const auto& entry : interface.addressEntries())
      if (entry.prefixLength() > 0 && entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
          address.isInSubnet(entry.ip(), entry.prefixLength()))
        return true;
  return false;
}

QVector<QHostAddress> RobotDiscovery::addresses() const {
  QVector<QHostAddress> result;
  for (const auto& entry : entries_)
    if (entry.expiresMs > clock_.elapsed())
      result.append(entry.address);
  return result;
}

void RobotDiscovery::finish(const QString& status) {
  active_ = false;
  retry_.stop();
  socket_.close();
  status_ = status;
  if (entries_.isEmpty())
    maintenance_.stop();
  emit changed();
}

void RobotDiscovery::maintain() {
  const auto now = clock_.elapsed();
  const auto size = entries_.size();
  entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                [now](const auto& entry) { return entry.expiresMs <= now; }),
                 entries_.end());
  if (active_ && now >= SearchTimeoutMs) {
    finish(
        entries_.isEmpty()
            ? QStringLiteral("No matching mDNS response. Manual hints are unverified.")
            : QStringLiteral("mDNS search finished. Select an address, then Connect explicitly."));
  } else if (size != entries_.size()) {
    if (!active_ && entries_.isEmpty())
      status_ =
          QStringLiteral("mDNS results expired. Search again or use an unverified manual hint.");
    emit changed();
  }
  if (!active_ && entries_.isEmpty())
    maintenance_.stop();
}

void RobotDiscovery::receive() {
  if (!active_)
    return;
  int budget = 32;
  while (active_ && socket_.hasPendingDatagrams() && budget-- > 0) {
    if (clock_.elapsed() >= SearchTimeoutMs) {
      maintain();
      return;
    }
    if (socket_.pendingDatagramSize() > discovery::MaxDatagram) {
      socket_.receiveDatagram(0);
      ++rejectedCount_;
      continue;
    }
    const auto packet = socket_.receiveDatagram(discovery::MaxDatagram);
    if (!packet.isValid() || packet.senderPort() != target_.port ||
        !allowedSender(packet.senderAddress())) {
      ++rejectedCount_;
      continue;
    }
    QString error;
    const auto records = discovery::parseResponse(packet.data(), team_, id_, error);
    if (!records) {
      ++rejectedCount_;
      continue;
    }
    for (const auto& record : *records) {
      auto entry = std::find_if(entries_.begin(), entries_.end(),
                                [&](const auto& value) { return value.address == record.address; });
      if (record.ttlSeconds == 0) {
        if (entry != entries_.end())
          entries_.erase(entry);
        continue;
      }
      const auto expires =
          clock_.elapsed() + 1000 * qMin(record.ttlSeconds, quint32(MaximumTtlSeconds));
      if (entry != entries_.end())
        entry->expiresMs = expires;
      else if (entries_.size() < discovery::MaxAddresses)
        entries_.append({record.address, expires});
    }
  }
  emit changed();
  if (active_ && socket_.hasPendingDatagrams() && !receiveScheduled_) {
    receiveScheduled_ = true;
    const auto generation = generation_;
    QTimer::singleShot(0, this, [this, generation] {
      if (generation != generation_)
        return;
      receiveScheduled_ = false;
      receive();
    });
  }
}
} // namespace ds
