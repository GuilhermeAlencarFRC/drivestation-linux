#include "robot_protocol.h"
#include <QDate>
#include <QTime>

namespace ds::wire {
namespace {
class Reader {
public:
  explicit Reader(const QByteArray& bytes) : bytes_(bytes) {}
  qsizetype remaining() const { return bytes_.size() - position_; }
  quint8 byte() { return static_cast<quint8>(bytes_[position_++]); }
  quint16 word() {
    const quint16 high = byte();
    return (high << 8) | byte();
  }
  quint32 dword() {
    const quint32 high = word();
    return (high << 16) | word();
  }
  QByteArray take(qsizetype count) {
    const auto result = bytes_.mid(position_, count);
    position_ += count;
    return result;
  }

private:
  const QByteArray& bytes_;
  qsizetype position_ = 0;
};
void byte(QByteArray& packet, quint8 value) { packet.append(static_cast<char>(value)); }
void word(QByteArray& packet, quint16 value) {
  byte(packet, value >> 8);
  byte(packet, value & 0xff);
}
bool validMode(Mode mode) { return static_cast<quint8>(mode) <= 2; }
bool validPov(qint16 value) { return value == -1 || (value >= 0 && value < 360); }
std::optional<QVector<QByteArray>> tags(Reader& reader, QString& error) {
  QVector<QByteArray> result;
  while (reader.remaining()) {
    const int size = reader.byte();
    if (size < 1 || reader.remaining() < size) {
      error = QStringLiteral("Invalid UDP tag length.");
      return {};
    }
    result.append(reader.take(size));
  }
  return result;
}
bool headerValid(const QByteArray& packet, int minimum, QString& error) {
  error.clear();
  if (packet.size() < minimum || packet.size() > MaxDatagram) {
    error = QStringLiteral("Invalid UDP datagram size.");
    return false;
  }
  if (static_cast<quint8>(packet[2]) != 1) {
    error = QStringLiteral("Unsupported UDP communication version.");
    return false;
  }
  return true;
}
} // namespace

qint8 quantizeAxis(qint16 value) {
  return static_cast<qint8>(value < 0 ? (value - 255) / 256 : value / 256);
}

bool validTimeData(const TimeData& time) {
  if (time.microseconds >= 1000000 ||
      !QDate(1900 + time.yearSince1900, time.month + 1, time.day).isValid() ||
      !QTime(time.hour, time.minute, time.second).isValid() || time.timezone.isEmpty() ||
      time.timezone.size() > MaxTimeZoneBytes)
    return false;
  for (const auto character : time.timezone)
    if (static_cast<quint8>(character) < 33 || static_cast<quint8>(character) > 126)
      return false;
  return true;
}

std::optional<QByteArray> encodeControl(const Control& control, QString& error) {
  error.clear();
  if (!validMode(control.mode) || control.station > 5 || control.joysticks.size() > MaxJoysticks) {
    error = QStringLiteral("Control mode, station or joystick count outside UDP profile.");
    return {};
  }
  QByteArray packet;
  word(packet, control.sequence);
  byte(packet, 1);
  byte(packet, static_cast<quint8>(control.mode) |
                   (control.enabled && !control.emergencyStopped ? 0x04 : 0) |
                   (control.emergencyStopped ? 0x80 : 0));
  byte(packet, 0); // No reboot, code restart or FMS requests.
  byte(packet, control.station);
  for (const auto& joystick : control.joysticks) {
    if (joystick.axes.size() > MaxAxes || joystick.buttons.size() > MaxButtons ||
        joystick.povs.size() > MaxPovs) {
      error = QStringLiteral("Joystick input exceeds UDP profile limits.");
      return {};
    }
    QByteArray data;
    byte(data, 0x0c);
    byte(data, static_cast<quint8>(joystick.axes.size()));
    for (auto axis : joystick.axes)
      byte(data, static_cast<quint8>(axis));
    byte(data, static_cast<quint8>(joystick.buttons.size()));
    quint32 buttons = 0;
    for (qsizetype i = 0; i < joystick.buttons.size(); ++i)
      if (joystick.buttons[i])
        buttons |= quint32(1) << i;
    const int bytes = (joystick.buttons.size() + 7) / 8;
    for (int i = bytes - 1; i >= 0; --i)
      byte(data, (buttons >> (i * 8)) & 0xff);
    byte(data, static_cast<quint8>(joystick.povs.size()));
    for (auto pov : joystick.povs) {
      if (!validPov(pov)) {
        error = QStringLiteral("Invalid joystick POV.");
        return {};
      }
      word(data, static_cast<quint16>(pov));
    }
    byte(packet, static_cast<quint8>(data.size()));
    packet.append(data);
  }
  if (control.time) {
    const auto& time = *control.time;
    if (!validTimeData(time)) {
      error = QStringLiteral("Invalid UDP UTC date/time or timezone.");
      return {};
    }
    byte(packet, 11); // ID plus ten date/time bytes.
    byte(packet, 0x0f);
    word(packet, time.microseconds >> 16);
    word(packet, time.microseconds & 0xffff);
    for (const auto value :
         {time.second, time.minute, time.hour, time.day, time.month, time.yearSince1900})
      byte(packet, value);
    byte(packet, static_cast<quint8>(time.timezone.size() + 1));
    byte(packet, 0x10);
    packet.append(time.timezone);
  }
  if (packet.size() > MaxDatagram) {
    error = QStringLiteral("UDP control exceeds the datagram limit.");
    return {};
  }
  return packet;
}

std::optional<Control> decodeControl(const QByteArray& packet, QString& error) {
  if (!headerValid(packet, 6, error))
    return {};
  Reader reader(packet);
  Control control;
  control.sequence = reader.word();
  reader.byte();
  const auto flags = reader.byte();
  control.mode = static_cast<Mode>(flags & 3);
  control.enabled = flags & 4;
  control.emergencyStopped = flags & 0x80;
  const auto request = reader.byte();
  control.station = reader.byte();
  if ((flags & ~0x87) || !validMode(control.mode) || request != 0 || control.station > 5 ||
      (control.enabled && control.emergencyStopped)) {
    error = QStringLiteral("Unsupported UDP control flags, request or station.");
    return {};
  }
  const auto parsedTags = tags(reader, error);
  if (!parsedTags)
    return {};
  std::optional<TimeData> time;
  std::optional<QByteArray> timezone;
  for (const auto& tag : *parsedTags) {
    Reader input(tag);
    const auto id = input.byte();
    if (id == 0x0f) {
      if (time || input.remaining() != 10) {
        error = QStringLiteral("Invalid or duplicate UDP date/time tag.");
        return {};
      }
      TimeData value;
      value.microseconds = input.dword();
      value.second = input.byte();
      value.minute = input.byte();
      value.hour = input.byte();
      value.day = input.byte();
      value.month = input.byte();
      value.yearSince1900 = input.byte();
      time = value;
      continue;
    }
    if (id == 0x10) {
      if (timezone || input.remaining() < 1 || input.remaining() > MaxTimeZoneBytes) {
        error = QStringLiteral("Invalid or duplicate UDP timezone tag.");
        return {};
      }
      timezone = input.take(input.remaining());
      continue;
    }
    if (id != 0x0c)
      continue;
    if (control.joysticks.size() == MaxJoysticks || input.remaining() < 3) {
      error = QStringLiteral("Invalid joystick tag.");
      return {};
    }
    Joystick joystick;
    const int axes = input.byte();
    if (axes > MaxAxes || input.remaining() < axes + 2) {
      error = QStringLiteral("Invalid joystick axes.");
      return {};
    }
    for (int i = 0; i < axes; ++i) {
      const int raw = input.byte();
      joystick.axes.append(static_cast<qint8>(raw < 128 ? raw : raw - 256));
    }
    const int count = input.byte();
    const int bytes = (count + 7) / 8;
    if (count > MaxButtons || input.remaining() < bytes + 1) {
      error = QStringLiteral("Invalid joystick buttons.");
      return {};
    }
    quint32 buttons = 0;
    for (int i = 0; i < bytes; ++i)
      buttons = (buttons << 8) | input.byte();
    if (count < 32 && (buttons >> count) != 0) {
      error = QStringLiteral("Nonzero unused joystick button bits.");
      return {};
    }
    for (int i = 0; i < count; ++i)
      joystick.buttons.append((buttons & (quint32(1) << i)) != 0);
    const int povs = input.byte();
    if (povs > MaxPovs || input.remaining() != povs * 2) {
      error = QStringLiteral("Invalid joystick POV count.");
      return {};
    }
    for (int i = 0; i < povs; ++i) {
      const int raw = input.word();
      const auto pov = static_cast<qint16>(raw < 32768 ? raw : raw - 65536);
      if (!validPov(pov)) {
        error = QStringLiteral("Invalid joystick POV.");
        return {};
      }
      joystick.povs.append(pov);
    }
    control.joysticks.append(joystick);
  }
  if (time.has_value() != timezone.has_value()) {
    error = QStringLiteral("UDP date/time and timezone must be paired in this profile.");
    return {};
  }
  if (time) {
    time->timezone = *timezone;
    if (!validTimeData(*time)) {
      error = QStringLiteral("Invalid UDP UTC date/time or timezone.");
      return {};
    }
    control.time = time;
  }
  return control;
}

std::optional<Status> decodeStatus(const QByteArray& packet, QString& error) {
  if (!headerValid(packet, 8, error))
    return {};
  Reader reader(packet);
  Status status;
  status.sequence = reader.word();
  reader.byte();
  const auto flags = reader.byte();
  status.mode = static_cast<Mode>(flags & 3);
  status.enabled = flags & 4;
  status.codeInitializing = flags & 8;
  status.brownout = flags & 0x10;
  status.emergencyStopped = flags & 0x80;
  if ((flags & ~0x9f) || !validMode(status.mode) || (status.enabled && status.emergencyStopped)) {
    error = QStringLiteral("Invalid UDP status flags.");
    return {};
  }
  status.trace = reader.byte();
  status.batteryRaw = reader.word();
  status.request = reader.byte();
  const auto parsedTags = tags(reader, error);
  if (!parsedTags)
    return {};
  for (const auto& tag : *parsedTags) {
    Reader input(tag);
    if (input.byte() != 1)
      continue;
    if (status.outputs.size() == MaxJoysticks ||
        (input.remaining() != 0 && input.remaining() != 8)) {
      error = QStringLiteral("Invalid joystick output tag.");
      return {};
    }
    JoystickOutput output;
    if (input.remaining()) {
      output.digital = input.dword();
      output.leftRumble = input.word();
      output.rightRumble = input.word();
    }
    status.outputs.append(output);
  }
  return status;
}

std::optional<QByteArray> encodeStatus(const Status& status, QString& error) {
  error.clear();
  if (!validMode(status.mode) || status.outputs.size() > MaxJoysticks) {
    error = QStringLiteral("Invalid status mode or joystick output count.");
    return {};
  }
  QByteArray packet;
  word(packet, status.sequence);
  byte(packet, 1);
  byte(packet, static_cast<quint8>(status.mode) |
                   (status.enabled && !status.emergencyStopped ? 4 : 0) |
                   (status.codeInitializing ? 8 : 0) | (status.brownout ? 0x10 : 0) |
                   (status.emergencyStopped ? 0x80 : 0));
  byte(packet, status.trace);
  word(packet, status.batteryRaw);
  byte(packet, status.request);
  for (const auto& output : status.outputs) {
    const bool empty = output.digital == 0 && output.leftRumble == 0 && output.rightRumble == 0;
    byte(packet, empty ? 1 : 9);
    byte(packet, 1);
    if (!empty) {
      word(packet, output.digital >> 16);
      word(packet, output.digital & 0xffff);
      word(packet, output.leftRumble);
      word(packet, output.rightRumble);
    }
  }
  return packet;
}
} // namespace ds::wire
