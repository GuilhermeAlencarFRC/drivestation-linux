#include "demo_session.h"

#include <cmath>

namespace ds {

DemoSession::DemoSession(QObject* parent) : QObject(parent) {}

bool DemoSession::enabled() const {
  return mode_ == Mode::Practice ? practice_.enabled() : enabled_;
}

bool DemoSession::busy() const {
  return enabled() || (mode_ == Mode::Practice && practice_.active());
}

bool DemoSession::canEnable() const { return connected_ && !emergencyStopped_ && !busy(); }

qint64 DemoSession::elapsedMs() const {
  return mode_ == Mode::Practice ? practice_.elapsedMs() : elapsedMs_;
}

QString DemoSession::phaseName(PracticeSequence::Phase phase) {
  switch (phase) {
  case PracticeSequence::Phase::Idle:
    return QStringLiteral("Ready");
  case PracticeSequence::Phase::Countdown:
    return QStringLiteral("Countdown");
  case PracticeSequence::Phase::Autonomous:
    return QStringLiteral("Autonomous");
  case PracticeSequence::Phase::Delay:
    return QStringLiteral("Delay");
  case PracticeSequence::Phase::Teleoperated:
    return QStringLiteral("Teleoperated");
  case PracticeSequence::Phase::EndGame:
    return QStringLiteral("End Game");
  case PracticeSequence::Phase::Complete:
    return QStringLiteral("Complete");
  case PracticeSequence::Phase::Cancelled:
    return QStringLiteral("Cancelled");
  }
  return {};
}

QString DemoSession::modeName(Mode mode) {
  switch (mode) {
  case Mode::Teleoperated:
    return QStringLiteral("Teleoperated");
  case Mode::Autonomous:
    return QStringLiteral("Autonomous");
  case Mode::Practice:
    return QStringLiteral("Practice");
  case Mode::Test:
    return QStringLiteral("Test");
  }
  return {};
}

QString DemoSession::status() const {
  if (emergencyStopped_)
    return QStringLiteral("Emergency Stopped");
  if (!connected_)
    return QStringLiteral("No Robot Communication");
  if (mode_ == Mode::Practice) {
    if (practice_.autoStopped())
      return QStringLiteral("Autonomous A-Stopped");
    return QStringLiteral("Practice %1").arg(phaseName(practice_.phase()));
  }
  return modeName(mode_) + (enabled() ? QStringLiteral(" Enabled") : QStringLiteral(" Disabled"));
}

double DemoSession::voltage() const {
  if (!connected_)
    return 0.0;
  return 12.6 - (enabled() ? 0.6 : 0.0) - 0.1 * std::sin(simulationSeconds());
}

double DemoSession::tripTime() const {
  return connected_ ? 3.0 + std::sin(simulationSeconds() * 0.8) : 0.0;
}

void DemoSession::setConnected(bool connected) {
  if (connected_ == connected)
    return;
  connected_ = connected;
  enabled_ = false;
  practice_.cancel();
  emit changed();
}

void DemoSession::setMode(Mode mode) {
  if (mode < Mode::Teleoperated || mode > Mode::Test || mode_ == mode)
    return;
  mode_ = mode;
  enabled_ = false;
  practice_.cancel();
  emit changed();
}

bool DemoSession::setPracticeTimings(const PracticeSequence::Timings& seconds) {
  if (!PracticeSequence::validTimings(seconds))
    return false;
  if (practiceSeconds_ == seconds)
    return true;
  disable();
  practiceSeconds_ = seconds;
  emit changed();
  return true;
}

bool DemoSession::enable() {
  if (!canEnable())
    return false;
  if (mode_ == Mode::Practice) {
    practice_.start(practiceSeconds_);
  } else {
    enabled_ = true;
    elapsedMs_ = 0;
  }
  emit changed();
  return true;
}

void DemoSession::disable() {
  if (!busy())
    return;
  enabled_ = false;
  practice_.cancel();
  emit changed();
}

void DemoSession::emergencyStop() {
  if (!connected_ || emergencyStopped_)
    return;
  enabled_ = false;
  practice_.cancel();
  emergencyStopped_ = true;
  emit changed();
}

bool DemoSession::autonomousStop() {
  if (mode_ != Mode::Practice || !connected_ || emergencyStopped_ || !practice_.autonomousStop())
    return false;
  emit changed();
  return true;
}

void DemoSession::reset() {
  connected_ = false;
  enabled_ = false;
  emergencyStopped_ = false;
  elapsedMs_ = 0;
  simulationMs_ = 0;
  practice_.reset();
  emit changed();
}

void DemoSession::advance(qint64 milliseconds) {
  if (milliseconds <= 0 || !connected_)
    return;
  // An event-loop stall must never silently catch up through an enable boundary.
  // The pure PracticeSequence can still be tested with arbitrarily large steps.
  if (milliseconds > MaxTickGapMs) {
    disable();
    return;
  }
  simulationMs_ += milliseconds;
  if (mode_ == Mode::Practice)
    practice_.advance(milliseconds);
  else if (enabled_)
    elapsedMs_ += milliseconds;
  emit changed();
}

} // namespace ds
