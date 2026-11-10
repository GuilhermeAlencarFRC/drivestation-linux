#include "robot_tcp_protocol.h"

#include <QStringDecoder>
#include <QtEndian>
#include <cmath>
#include <cstring>

namespace ds::tcp {
namespace {
void byte(QByteArray& data, quint8 value) { data.append(static_cast<char>(value)); }
void word(QByteArray& data, quint16 value) {
  byte(data, value >> 8);
  byte(data, value & 0xff);
}

class Reader {
public:
  explicit Reader(const QByteArray& data) : data_(data) {}
  qsizetype remaining() const { return data_.size() - position_; }
  quint8 byte() { return static_cast<quint8>(data_[position_++]); }
  quint16 word() {
    const auto high = byte();
    return (quint16(high) << 8) | byte();
  }
  quint32 dword() {
    const auto high = word();
    return (quint32(high) << 16) | word();
  }
  std::optional<QString> text(qsizetype length) {
    if (length > remaining())
      return {};
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    QString value = decoder(data_.mid(position_, length));
    position_ += length;
    if (decoder.hasError())
      return {};
    // Console data stays plain text; remove non-printing control characters.
    for (auto& character : value)
      if ((character.unicode() < 32 && character != QLatin1Char('\n') &&
           character != QLatin1Char('\t') && character != QLatin1Char('\r')) ||
          character.unicode() == 127)
        character = QChar::ReplacementCharacter;
    return value;
  }
  std::optional<QString> shortText() { return remaining() >= 1 ? text(byte()) : std::nullopt; }
  std::optional<QString> longText() { return remaining() >= 2 ? text(word()) : std::nullopt; }

private:
  const QByteArray& data_;
  qsizetype position_ = 0;
};
} // namespace

bool StreamDecoder::feed(const QByteArray& bytes, QString& error) {
  error.clear();
  if (bytes.size() > MaxBuffer - buffer_.size()) {
    error = QStringLiteral("TCP receive buffer limit exceeded.");
    clear();
    return false;
  }
  buffer_.append(bytes);
  return true;
}

std::optional<Frame> StreamDecoder::take(QString& error) {
  error.clear();
  if (buffer_.size() < 2)
    return {};
  const int length = qFromBigEndian<quint16>(buffer_.constData());
  if (length > MaxFrame) {
    error = QStringLiteral("TCP frame exceeds the experimental profile limit.");
    clear();
    return {};
  }
  if (buffer_.size() < length + 2)
    return {};
  Frame frame;
  frame.heartbeat = length == 0;
  if (length) {
    frame.id = static_cast<quint8>(buffer_[2]);
    frame.payload = buffer_.mid(3, length - 1);
  }
  buffer_.remove(0, length + 2);
  return frame;
}

std::optional<QByteArray> encodeFrame(quint8 id, const QByteArray& payload, QString& error) {
  error.clear();
  if (payload.size() > MaxFrame - 1) {
    error = QStringLiteral("TCP outgoing frame is too large.");
    return {};
  }
  QByteArray frame;
  word(frame, static_cast<quint16>(payload.size() + 1));
  byte(frame, id);
  frame.append(payload);
  return frame;
}

std::optional<QByteArray> encodeDescriptors(const QVector<JoystickDescriptor>& descriptors,
                                            QString& error) {
  error.clear();
  if (descriptors.size() != wire::MaxJoysticks) {
    error = QStringLiteral("TCP requires descriptors for exactly six joystick slots.");
    return {};
  }
  QByteArray result;
  for (int slot = 0; slot < wire::MaxJoysticks; ++slot) {
    const auto& descriptor = descriptors[slot];
    const auto name = descriptor.name.toUtf8();
    if (name.size() > 255 || name.contains('\0') || descriptor.axisTypes.size() > wire::MaxAxes ||
        descriptor.buttons > wire::MaxButtons || descriptor.povs > wire::MaxPovs) {
      error = QStringLiteral("TCP joystick descriptor exceeds the supported limits.");
      return {};
    }
    QByteArray payload;
    byte(payload, slot);
    byte(payload, 0);    // No claim of NI Xbox mapping for raw SDL inputs.
    byte(payload, 0xff); // Unknown joystick type (-1).
    byte(payload, static_cast<quint8>(name.size()));
    payload.append(name);
    byte(payload, static_cast<quint8>(descriptor.axisTypes.size()));
    for (const auto type : descriptor.axisTypes)
      byte(payload, type);
    byte(payload, descriptor.buttons);
    byte(payload, descriptor.povs);
    const auto frame = encodeFrame(0x02, payload, error);
    if (!frame)
      return {};
    result.append(*frame);
  }
  return result;
}

std::optional<QByteArray> encodeGameData(const QString& data, QString& error) {
  const auto bytes = data.toUtf8();
  if (bytes.size() > MaxGameData) {
    error = QStringLiteral("TCP game data exceeds 1024 UTF-8 bytes.");
    return {};
  }
  return encodeFrame(0x0e, bytes, error);
}

std::optional<Message> decodeMessage(const Frame& frame, QString& error) {
  error.clear();
  Message message;
  if (frame.heartbeat)
    return message;
  Reader reader(frame.payload);
  const auto invalid = [&]() -> std::optional<Message> {
    error = QStringLiteral("Malformed TCP tag 0x%1.").arg(frame.id, 2, 16, QLatin1Char('0'));
    return {};
  };
  if (frame.payload.size() > MaxFrame - 1)
    return invalid();
  if (frame.id == 0x0c || frame.id == 0x0b) {
    if (reader.remaining() < 6)
      return invalid();
    const auto bits = reader.dword();
    static_assert(sizeof(message.seconds) == sizeof(bits));
    std::memcpy(&message.seconds, &bits, sizeof(bits));
    if (!std::isfinite(message.seconds) || message.seconds < 0)
      return invalid();
    message.sequence = reader.word();
    if (frame.id == 0x0c) {
      const auto text = reader.text(reader.remaining());
      if (!text)
        return invalid();
      message.kind = Message::Kind::Stdout;
      message.text = *text;
    } else {
      if (reader.remaining() < 7)
        return invalid();
      reader.word(); // Reserved field in the documented profile.
      const auto code = reader.dword();
      message.errorCode = code <= 0x7fffffffU ? static_cast<qint32>(code)
                                              : static_cast<qint32>(qint64(code) - 0x100000000LL);
      const auto flags = reader.byte();
      if (flags & ~3)
        return invalid();
      message.isError = flags & 1;
      const auto details = reader.longText();
      const auto location = reader.longText();
      const auto stack = reader.longText();
      if (!details || !location || !stack || reader.remaining() != 0)
        return invalid();
      message.kind = Message::Kind::Error;
      message.text = *details;
      message.location = *location;
      message.stack = *stack;
    }
  } else if (frame.id == 0x0a) {
    if (reader.remaining() < 6)
      return invalid();
    message.kind = Message::Kind::Version;
    message.deviceType = reader.byte();
    reader.word(); // Reserved.
    message.deviceId = reader.byte();
    const auto name = reader.shortText();
    const auto version = reader.shortText();
    if (!name || !version || reader.remaining() != 0)
      return invalid();
    message.name = *name;
    message.version = *version;
    message.endOfVersions = frame.payload == QByteArray(6, '\0');
  }
  return message;
}
} // namespace ds::tcp
