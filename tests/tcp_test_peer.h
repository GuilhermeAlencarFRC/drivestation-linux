#pragma once

#include "robot_tcp_protocol.h"
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <memory>

namespace ds::test {
class TcpPeer : public QObject {
public:
  QTcpServer server;
  QPointer<QTcpSocket> socket;
  QVector<tcp::Frame> frames;
  QString error;
  int connections = 0;
  TcpPeer() {
    server.listen(QHostAddress::LocalHost, 0);
    connect(&server, &QTcpServer::newConnection, this, [this] {
      socket = server.nextPendingConnection();
      ++connections;
      const auto connection = socket;
      const auto decoder = std::make_shared<tcp::StreamDecoder>();
      connect(socket, &QTcpSocket::readyRead, this, [this, connection, decoder] {
        if (!connection || !decoder->feed(connection->readAll(), error))
          return;
        while (const auto frame = decoder->take(error))
          frames.append(*frame);
      });
    });
  }
  void send(const QByteArray& data) {
    if (socket)
      socket->write(data);
  }
};
} // namespace ds::test
