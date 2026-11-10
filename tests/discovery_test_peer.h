#pragma once

#include <QNetworkDatagram>
#include <QUdpSocket>
#include <QVector>

namespace ds::test {
inline QByteArray dnsReply(const QByteArray& query, const QVector<quint32>& addresses,
                           quint32 ttl = 10) {
  if (query.size() < 12)
    return {};
  auto reply = query;
  reply[2] = char(0x84);
  reply[3] = 0;
  reply[6] = char(addresses.size() >> 8);
  reply[7] = char(addresses.size());
  const auto word = [&](quint16 value) {
    reply.append(char(value >> 8));
    reply.append(char(value));
  };
  for (const auto address : addresses) {
    reply.append(QByteArray::fromHex("c00c00010001")); // Owner points to the repeated question.
    word(ttl >> 16);
    word(ttl & 0xffff);
    word(4);
    word(address >> 16);
    word(address & 0xffff);
  }
  return reply;
}

class DiscoveryPeer : public QObject {
public:
  QUdpSocket socket;
  QVector<QByteArray> queries;
  QNetworkDatagram last;
  bool replying = true;
  QVector<quint32> addresses{0x7f000001};
  quint32 ttl = 10;
  DiscoveryPeer() {
    socket.bind(QHostAddress::LocalHost, 0, QUdpSocket::DontShareAddress);
    connect(&socket, &QUdpSocket::readyRead, this, [this] {
      while (socket.hasPendingDatagrams()) {
        last = socket.receiveDatagram();
        queries.append(last.data());
        if (replying)
          send(dnsReply(last.data(), addresses, ttl));
      }
    });
  }
  void send(const QByteArray& response) {
    socket.writeDatagram(response, last.senderAddress(), last.senderPort());
  }
};
} // namespace ds::test
