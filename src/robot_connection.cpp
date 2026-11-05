#include "robot_connection.h"
#include <algorithm>
#include <iterator>

namespace ds {
void RobotConnection::start(quint16 sequence, qint64 nowMs) {
  stop();
  control_.sequence = sequence;
  active_ = true;
  lastSentMs_ = nowMs;
  lastReceivedMs_ = nowMs;
}

void RobotConnection::stop() {
  active_ = false;
  disable();
  status_.reset();
  pending_.clear();
  lastAccepted_.reset();
  roundTripMs_.reset();
  ordinal_ = 0;
}

void RobotConnection::prune(qint64 nowMs) {
  while (!pending_.empty() && nowMs - pending_.front().sentMs >= ResponseTimeoutMs)
    pending_.pop_front();
}

void RobotConnection::expire(qint64 nowMs) {
  prune(nowMs);
  if (status_ && nowMs - lastReceivedMs_ >= ResponseTimeoutMs) {
    status_.reset();
    roundTripMs_.reset();
    disable();
  }
  if (active_ && nowMs - lastSentMs_ >= MaxSendGapMs) {
    disable();
    status_.reset();
    roundTripMs_.reset();
    pending_.clear();
  }
}

wire::Control RobotConnection::nextControl(qint64 nowMs) {
  expire(nowMs);
  auto outgoing = control_;
  outgoing.enabled = active_ && enableRequested_;
  outgoing.emergencyStopped = emergencyStopped_;
  if (active_) {
    pending_.push_back({control_.sequence, ordinal_++, nowMs});
    if (pending_.size() > 32)
      pending_.pop_front();
    ++control_.sequence;
    lastSentMs_ = nowMs;
  }
  return outgoing;
}

bool RobotConnection::accept(wire::Status status, qint64 nowMs) {
  expire(nowMs);
  if (!active_)
    return false;
  const auto sent = std::find_if(pending_.begin(), pending_.end(), [&](const auto& pending) {
    return pending.sequence == status.sequence;
  });
  if (sent == pending_.end() || (lastAccepted_ && sent->ordinal <= *lastAccepted_))
    return false;
  lastAccepted_ = sent->ordinal;
  roundTripMs_ = nowMs - sent->sentMs;
  lastReceivedMs_ = nowMs;
  status_ = status;
  if (status.emergencyStopped)
    emergencyStop();
  if (!status.hasRobotCode() || status.codeInitializing)
    disable();
  pending_.erase(pending_.begin(), std::next(sent));
  return true;
}

bool RobotConnection::requestEnable(qint64 nowMs) {
  expire(nowMs);
  if (!active_ || !status_ || !status_->hasRobotCode() || status_->codeInitializing ||
      emergencyStopped_)
    return false;
  enableRequested_ = true;
  return true;
}

void RobotConnection::emergencyStop() {
  emergencyStopped_ = true;
  disable();
}

bool RobotConnection::setMode(wire::Mode mode) {
  if (static_cast<quint8>(mode) > 2) {
    disable();
    return false;
  }
  if (control_.mode != mode)
    disable();
  control_.mode = mode;
  return true;
}

bool RobotConnection::setStation(quint8 station) {
  if (station > 5) {
    disable();
    return false;
  }
  if (control_.station != station)
    disable();
  control_.station = station;
  return true;
}

bool RobotConnection::setJoysticks(const QVector<wire::Joystick>& joysticks, QString& error) {
  auto candidate = control_;
  candidate.joysticks = joysticks;
  if (!wire::encodeControl(candidate, error)) {
    control_.joysticks.clear();
    disable();
    return false;
  }
  control_.joysticks = joysticks;
  return true;
}
} // namespace ds
