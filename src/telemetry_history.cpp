#include "telemetry_history.h"
#include "robot_connection.h"
#include <QJsonArray>

namespace ds {
void TelemetryHistory::clear() {
  samples_.clear();
  messages_.clear();
  lastReceivedMs_.reset();
  segment_ = 0;
  droppedSamples_ = droppedMessages_ = 0;
}

std::optional<TelemetrySample> TelemetryHistory::append(qint64 elapsedMs, qint64 roundTripMs,
                                                        const wire::Status& status) {
  if (elapsedMs < 0 || roundTripMs < 0 ||
      (lastReceivedMs_ && elapsedMs < *lastReceivedMs_) ||
      (!samples_.empty() && elapsedMs < samples_.back().elapsedMs))
    return {};
  const bool gap = !lastReceivedMs_ ||
                   elapsedMs - *lastReceivedMs_ >= RobotConnection::ResponseTimeoutMs;
  lastReceivedMs_ = elapsedMs; // Include replies omitted by the chart's sampling interval.
  if (!gap && !samples_.empty() &&
      elapsedMs - samples_.back().elapsedMs < SampleIntervalMs)
    return {};
  if (gap && !samples_.empty())
    ++segment_;
  // Joystick outputs are deliberately not retained in diagnostics.
  auto received = status;
  received.outputs.clear();
  const TelemetrySample sample{elapsedMs, roundTripMs, received, segment_, gap};
  samples_.push_back(sample);
  if (samples_.size() > MaxSamples) {
    samples_.pop_front();
    ++droppedSamples_;
  }
  return sample;
}

void TelemetryHistory::addMessage(QJsonObject message) {
  messages_.push_back(std::move(message));
  if (messages_.size() > MaxMessages) {
    messages_.pop_front();
    ++droppedMessages_;
  }
}

void TelemetryHistory::event(qint64 elapsedMs, const QString& text) {
  addMessage({{"elapsed_ms", elapsedMs}, {"kind", "station"},
              {"text", text.left(MaxTextChars)}, {"truncated", text.size() > MaxTextChars}});
}

void TelemetryHistory::message(qint64 elapsedMs, const tcp::Message& message) {
  if (message.kind == tcp::Message::Kind::Ignored)
    return;
  const auto kind = message.kind == tcp::Message::Kind::Version ? "version"
                    : message.kind == tcp::Message::Kind::Stdout ? "stdout"
                    : message.isError ? "error" : "warning";
  addMessage({{"elapsed_ms", elapsedMs}, {"kind", kind},
              {"robot_seconds", double(message.seconds)}, {"sequence", message.sequence},
              {"error_code", message.errorCode}, {"text", message.text.left(MaxTextChars)},
              {"location", message.location.left(MaxTextChars)},
              {"stack", message.stack.left(MaxTextChars)},
              {"device_type", message.deviceType}, {"device_id", message.deviceId},
              {"name", message.name.left(MaxTextChars)},
              {"version", message.version.left(MaxTextChars)},
              {"truncated", message.text.size() > MaxTextChars ||
                                message.location.size() > MaxTextChars ||
                                message.stack.size() > MaxTextChars ||
                                message.name.size() > MaxTextChars ||
                                message.version.size() > MaxTextChars}});
}

QJsonObject TelemetryHistory::toJson() const {
  QJsonArray samples;
  for (const auto& sample : samples_) {
    const auto& status = sample.status;
    samples.append(QJsonObject{{"elapsed_ms", sample.elapsedMs},
                               {"round_trip_ms", sample.roundTripMs},
                               {"battery_volts", status.voltage()},
                               {"sequence", status.sequence}, {"segment", sample.segment},
                               {"mode", int(status.mode)}, {"enabled", status.enabled},
                               {"emergency_stopped", status.emergencyStopped},
                               {"brownout", status.brownout},
                               {"robot_code", status.hasRobotCode()},
                               {"code_initializing", status.codeInitializing}});
  }
  QJsonArray messages;
  for (const auto& message : messages_)
    messages.append(message);
  return {{"sample_interval_ms", SampleIntervalMs}, {"max_samples", MaxSamples},
           {"dropped_samples", droppedSamples_}, {"dropped_messages", droppedMessages_},
           {"max_messages", MaxMessages}, {"max_text_chars", MaxTextChars},
           {"samples", samples}, {"messages", messages}};
}
} // namespace ds
