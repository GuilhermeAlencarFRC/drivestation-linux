#pragma once

#include "robot_protocol.h"
#include <deque>

namespace ds {
// Deterministic connection policy; all timestamps are monotonic milliseconds.
class RobotConnection {
public:
  static constexpr qint64 ResponseTimeoutMs = 250;
  static constexpr qint64 MaxSendGapMs = 100;
  void start(quint16 sequence, qint64 nowMs);
  void stop();
  wire::Control nextControl(qint64 nowMs);
  bool accept(wire::Status status, qint64 nowMs);
  void expire(qint64 nowMs);
  bool requestEnable(qint64 nowMs);
  void disable() { enableRequested_ = false; }
  void emergencyStop();
  bool setMode(wire::Mode mode);
  bool setStation(quint8 station);
  bool setJoysticks(const QVector<wire::Joystick>& joysticks, QString& error);
  bool active() const { return active_; }
  bool connected() const { return status_.has_value(); }
  bool enableRequested() const { return enableRequested_; }
  bool emergencyStopped() const { return emergencyStopped_; }
  const std::optional<wire::Status>& status() const { return status_; }
  std::optional<qint64> roundTripMs() const { return roundTripMs_; }

private:
  struct Pending {
    quint16 sequence;
    quint64 ordinal;
    qint64 sentMs;
  };
  void prune(qint64 nowMs);
  wire::Control control_;
  std::deque<Pending> pending_;
  std::optional<wire::Status> status_;
  std::optional<quint64> lastAccepted_;
  std::optional<qint64> roundTripMs_;
  quint64 ordinal_ = 0;
  qint64 lastReceivedMs_ = 0;
  qint64 lastSentMs_ = 0;
  bool active_ = false;
  bool enableRequested_ = false;
  bool emergencyStopped_ = false;
};
} // namespace ds
