#pragma once

#include "robot_protocol.h"
#include <QDateTime>

namespace ds {
// Monotonic scheduling is independent of the wall clock encoded for the robot.
class ClockSync {
public:
  static constexpr qint64 MinimumIntervalMs = 1000;
  void reset() {
    pending_ = true;
    lastSentMs_.reset();
  }
  void request() { pending_ = true; }
  bool due(qint64 nowMs) const {
    return pending_ && (!lastSentMs_ || nowMs - *lastSentMs_ >= MinimumIntervalMs);
  }
  void sent(qint64 nowMs) {
    pending_ = false;
    lastSentMs_ = nowMs;
  }

private:
  bool pending_ = false;
  std::optional<qint64> lastSentMs_;
};

std::optional<wire::TimeData> makeTimeData(const QDateTime& dateTime, const QByteArray& timezone,
                                           QString& error);
std::optional<wire::TimeData> systemTimeData(QString& error);
} // namespace ds
