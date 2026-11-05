#include "demo_session.h"

#include <cmath>

namespace ds {

DemoSession::DemoSession(QObject* parent) : QObject(parent) {}

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
  return modeName(mode_) + (enabled_ ? QStringLiteral(" Enabled") : QStringLiteral(" Disabled"));
}

double DemoSession::voltage() const {
  if (!connected_)
    return 0.0;
  return 12.6 - (enabled_ ? 0.6 : 0.0) - 0.1 * std::sin(simulationSeconds());
}

double DemoSession::tripTime() const {
  return connected_ ? 3.0 + std::sin(simulationSeconds() * 0.8) : 0.0;
}

void DemoSession::setConnected(bool connected) {
  if (connected_ == connected)
    return;
  connected_ = connected;
  enabled_ = false;
  emit changed();
}

void DemoSession::setMode(Mode mode) {
  if (mode_ == mode)
    return;
  mode_ = mode;
  enabled_ = false;
  emit changed();
}

bool DemoSession::enable() {
  if (!connected_ || emergencyStopped_ || mode_ == Mode::Practice)
    return false;
  if (!enabled_) {
    enabled_ = true;
    elapsedMs_ = 0;
    emit changed();
  }
  return true;
}

void DemoSession::disable() {
  if (!enabled_)
    return;
  enabled_ = false;
  emit changed();
}

void DemoSession::emergencyStop() {
  if (!connected_ || emergencyStopped_)
    return;
  enabled_ = false;
  emergencyStopped_ = true;
  emit changed();
}

void DemoSession::reset() {
  connected_ = false;
  enabled_ = false;
  emergencyStopped_ = false;
  elapsedMs_ = 0;
  simulationMs_ = 0;
  emit changed();
}

void DemoSession::advance(qint64 milliseconds) {
  if (milliseconds <= 0 || !connected_)
    return;
  simulationMs_ += milliseconds;
  if (enabled_)
    elapsedMs_ += milliseconds;
  emit changed();
}

} // namespace ds
