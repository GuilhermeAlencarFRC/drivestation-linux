#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <optional>

namespace ds::wire {
constexpr int MaxDatagram = 1024;
constexpr int MaxJoysticks = 6;
constexpr int MaxAxes = 12;
constexpr int MaxButtons = 32;
constexpr int MaxPovs = 12;
constexpr quint16 RobotPort = 1110;
constexpr quint16 StationPort = 1150;
constexpr int SendIntervalMs = 20;

enum class Mode : quint8 { Teleoperated = 0, Test = 1, Autonomous = 2 };
struct Joystick {
  QVector<qint8> axes;
  QVector<bool> buttons;
  QVector<qint16> povs;
};
struct Control {
  quint16 sequence = 0;
  Mode mode = Mode::Teleoperated;
  bool enabled = false;
  bool emergencyStopped = false;
  quint8 station = 0;
  QVector<Joystick> joysticks;
};
struct JoystickOutput {
  quint32 digital = 0;
  quint16 leftRumble = 0;
  quint16 rightRumble = 0;
};
struct Status {
  quint16 sequence = 0;
  Mode mode = Mode::Teleoperated;
  bool enabled = false;
  bool emergencyStopped = false;
  bool brownout = false;
  bool codeInitializing = false;
  quint8 trace = 0;
  quint16 batteryRaw = 0;
  quint8 request = 0;
  QVector<JoystickOutput> outputs;
  bool hasRobotCode() const { return (trace & 0x20) != 0; }
  double voltage() const { return batteryRaw / 256.0; }
};

qint8 quantizeAxis(qint16 value);
std::optional<QByteArray> encodeControl(const Control& control, QString& error);
std::optional<Control> decodeControl(const QByteArray& packet, QString& error);
std::optional<Status> decodeStatus(const QByteArray& packet, QString& error);
// Used by the local test peer, not by the station transmitter.
std::optional<QByteArray> encodeStatus(const Status& status, QString& error);
} // namespace ds::wire
