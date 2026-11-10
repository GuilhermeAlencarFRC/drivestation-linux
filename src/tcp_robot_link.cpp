#include "tcp_robot_link.h"
#include <algorithm>

namespace ds {
TcpRobotLink::TcpRobotLink(QObject* parent) : QObject(parent), socket_(this), watchdog_(this) {
  socket_.setReadBufferSize(tcp::MaxBuffer);
  watchdog_.setInterval(50);
  connect(&watchdog_, &QTimer::timeout, this, &TcpRobotLink::checkDeadlines);
  connect(&socket_, &QTcpSocket::connected, this, [this] {
    socket_.setSocketOption(QAbstractSocket::LowDelayOption, 1);
    synchronize();
  });
  connect(&socket_, &QTcpSocket::bytesWritten, this, [this] {
    if (active_ && connected() && !synchronized_ && socket_.bytesToWrite() == 0) {
      synchronized_ = true;
      emit changed();
    }
  });
  connect(&socket_, &QTcpSocket::readyRead, this, &TcpRobotLink::receive);
  connect(&socket_, &QTcpSocket::disconnected, this, [this] {
    if (active_)
      fail(QStringLiteral("TCP robot disconnected."));
  });
  connect(&socket_, &QTcpSocket::errorOccurred, this, [this] {
    if (active_)
      fail(socket_.errorString());
  });
  QString ignored;
  stationData_ =
      *tcp::encodeDescriptors(QVector<tcp::JoystickDescriptor>(wire::MaxJoysticks), ignored);
  stationData_.append(*tcp::encodeGameData({}, ignored));
}

TcpRobotLink::~TcpRobotLink() { stop(); }

bool TcpRobotLink::start(const QHostAddress& peer, quint16 port) {
  stop();
  lastError_.clear();
  sentCount_ = receivedCount_ = 0;
  if (peer.protocol() != QAbstractSocket::IPv4Protocol || peer.isNull() || peer.isMulticast() ||
      peer == QHostAddress::AnyIPv4 || peer == QHostAddress::Broadcast || port == 0) {
    fail(QStringLiteral("TCP requires an explicit unicast IPv4 endpoint and nonzero port."));
    return false;
  }
  active_ = true;
  clock_.start();
  windowSinceMs_ = 0;
  windowFrames_ = 0;
  windowBytes_ = 0;
  watchdog_.start();
  socket_.connectToHost(peer, port);
  emit changed();
  return active_;
}

void TcpRobotLink::stop() {
  const bool changed = active_ || synchronized_ || !versions_.isEmpty();
  active_ = false; // Socket signals may fire synchronously during abort.
  synchronized_ = false;
  watchdog_.stop();
  socket_.abort();
  decoder_.clear();
  partialSinceMs_ = -1;
  versions_.clear();
  if (changed)
    emit this->changed();
}

void TcpRobotLink::fail(const QString& error) {
  lastError_ = error;
  stop();
  emit errorOccurred(error);
  emit changed();
}

bool TcpRobotLink::setStationData(const QVector<tcp::JoystickDescriptor>& descriptors,
                                  const QString& gameData) {
  QString error;
  const auto encoded = tcp::encodeDescriptors(descriptors, error);
  if (!encoded) {
    fail(error);
    return false;
  }
  const auto game = tcp::encodeGameData(gameData, error);
  if (!game) {
    fail(error);
    return false;
  }
  const auto snapshot = *encoded + *game;
  if (snapshot == stationData_)
    return true;
  stationData_ = snapshot;
  if (connected())
    synchronize();
  return true;
}

void TcpRobotLink::synchronize() {
  if (!active_ || !connected())
    return;
  synchronized_ = false;
  emit changed(); // The owner cancels any existing Enable before changed descriptors leave.
  if (!active_ || !connected())
    return;
  if (socket_.bytesToWrite() + stationData_.size() > MaxQueuedWrite) {
    fail(QStringLiteral("TCP send queue limit exceeded."));
    return;
  }
  if (socket_.write(stationData_) != stationData_.size()) {
    fail(socket_.errorString());
    return;
  }
  sentCount_ += wire::MaxJoysticks + 1;
}

void TcpRobotLink::checkDeadlines() {
  if (!active_)
    return;
  if (!connected() && clock_.elapsed() >= ConnectTimeoutMs)
    fail(QStringLiteral("TCP connection timed out."));
  else if (partialSinceMs_ >= 0 && clock_.elapsed() - partialSinceMs_ >= PartialFrameTimeoutMs)
    fail(QStringLiteral("Incomplete TCP frame timed out."));
}

void TcpRobotLink::receive() {
  if (!active_ || !connected())
    return;
  // Bound work per callback as well as memory. Floods cannot grow the Qt read/write queues.
  int frames = 64;
  int bytes = tcp::MaxBuffer;
  QString error;
  while (active_ && frames > 0) {
    const auto frame = decoder_.take(error);
    if (!error.isEmpty()) {
      fail(error);
      return;
    }
    if (frame) {
      --frames;
      partialSinceMs_ = -1;
      if (clock_.elapsed() - windowSinceMs_ >= 1000) {
        windowSinceMs_ = clock_.elapsed();
        windowFrames_ = 0;
        windowBytes_ = 0;
      }
      if (++windowFrames_ > MaxFramesPerSecond) {
        fail(QStringLiteral("TCP message rate limit exceeded."));
        return;
      }
      const auto message = tcp::decodeMessage(*frame, error);
      if (!message) {
        fail(error);
        return;
      }
      ++receivedCount_;
      if (message->kind == tcp::Message::Kind::Version && !message->endOfVersions) {
        auto existing = std::find_if(versions_.begin(), versions_.end(), [&](const auto& version) {
          return version.deviceType == message->deviceType &&
                 version.deviceId == message->deviceId && version.name == message->name;
        });
        if (existing != versions_.end())
          *existing = *message;
        else if (versions_.size() < 32)
          versions_.append(*message);
      }
      if (message->kind != tcp::Message::Kind::Ignored)
        emit messageReceived(*message);
      continue;
    }
    if (decoder_.bufferedBytes() > 0 && partialSinceMs_ < 0)
      partialSinceMs_ = clock_.elapsed();
    if (socket_.bytesAvailable() == 0 || bytes == 0)
      break;
    const auto chunk = socket_.read(qMin(bytes, 4096));
    if (chunk.isEmpty())
      break;
    bytes -= static_cast<int>(chunk.size());
    if (clock_.elapsed() - windowSinceMs_ >= 1000) {
      windowSinceMs_ = clock_.elapsed();
      windowFrames_ = 0;
      windowBytes_ = 0;
    }
    windowBytes_ += chunk.size();
    if (windowBytes_ > MaxBytesPerSecond) {
      fail(QStringLiteral("TCP byte rate limit exceeded."));
      return;
    }
    if (!decoder_.feed(chunk, error)) {
      fail(error);
      return;
    }
  }
  if (active_)
    emit changed();
  if (active_ && (socket_.bytesAvailable() > 0 || frames == 0) && !receiveScheduled_) {
    receiveScheduled_ = true;
    QTimer::singleShot(0, this, [this] {
      receiveScheduled_ = false;
      receive();
    });
  }
}
} // namespace ds
