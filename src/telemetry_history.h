#pragma once

#include "robot_tcp_protocol.h"
#include <QJsonObject>
#include <deque>

namespace ds {
struct TelemetrySample {
  qint64 elapsedMs = 0;
  qint64 roundTripMs = 0;
  wire::Status status;
  int segment = 0;
  bool startsSegment = false;
};

// Only accepted robot replies enter this history. It never generates telemetry.
class TelemetryHistory {
public:
  static constexpr int SampleIntervalMs = 200;
  static constexpr int MaxSamples = 3000;
  static constexpr int MaxMessages = 256;
  static constexpr int MaxTextChars = 2048;
  void clear();
  void breakSegment() { lastReceivedMs_.reset(); }
  std::optional<TelemetrySample> append(qint64 elapsedMs, qint64 roundTripMs,
                                         const wire::Status& status);
  void message(qint64 elapsedMs, const tcp::Message& message);
  void event(qint64 elapsedMs, const QString& text);
  const std::deque<TelemetrySample>& samples() const { return samples_; }
  QJsonObject toJson() const;

private:
  void addMessage(QJsonObject message);
  std::deque<TelemetrySample> samples_;
  std::deque<QJsonObject> messages_;
  std::optional<qint64> lastReceivedMs_;
  int segment_ = 0;
  qint64 droppedSamples_ = 0;
  qint64 droppedMessages_ = 0;
};
} // namespace ds
