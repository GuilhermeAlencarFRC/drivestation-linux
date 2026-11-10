#pragma once

#include "robot_protocol.h"

namespace ds::tcp {
constexpr quint16 RobotPort = 1740;
constexpr int MaxFrame = 8192; // Includes the tag ID, excludes the two-byte length.
constexpr int MaxBuffer = 2 * (MaxFrame + 2);
constexpr int MaxGameData = 1024;

struct Frame {
  bool heartbeat = false;
  quint8 id = 0;
  QByteArray payload;
};

class StreamDecoder {
public:
  bool feed(const QByteArray& bytes, QString& error);
  std::optional<Frame> take(QString& error);
  qsizetype bufferedBytes() const { return buffer_.size(); }
  void clear() { buffer_.clear(); }

private:
  QByteArray buffer_;
};

struct JoystickDescriptor {
  // Raw SDL order: no verified NI/XInput mapping. Unknown type/axis usage are explicit.
  QString name;
  QVector<quint8> axisTypes;
  quint8 buttons = 0;
  quint8 povs = 0;
};

struct Message {
  enum class Kind { Ignored, Stdout, Error, Version };
  Kind kind = Kind::Ignored;
  float seconds = 0;
  quint16 sequence = 0;
  qint32 errorCode = 0;
  bool isError = false;
  QString text;
  QString location;
  QString stack;
  quint8 deviceType = 0;
  quint8 deviceId = 0;
  QString name;
  QString version;
  bool endOfVersions = false;
};

std::optional<QByteArray> encodeFrame(quint8 id, const QByteArray& payload, QString& error);
std::optional<QByteArray> encodeDescriptors(const QVector<JoystickDescriptor>& descriptors,
                                            QString& error);
std::optional<QByteArray> encodeGameData(const QString& data, QString& error);
std::optional<Message> decodeMessage(const Frame& frame, QString& error);
} // namespace ds::tcp
