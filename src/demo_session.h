#pragma once

#include <QObject>

namespace ds {

// A local UI demonstration. This class has no transport or hardware access.
class DemoSession : public QObject {
  Q_OBJECT
public:
  enum class Mode { Teleoperated, Autonomous, Practice, Test };
  Q_ENUM(Mode)

  explicit DemoSession(QObject* parent = nullptr);
  bool connected() const { return connected_; }
  bool enabled() const { return enabled_; }
  bool emergencyStopped() const { return emergencyStopped_; }
  Mode mode() const { return mode_; }
  qint64 elapsedMs() const { return elapsedMs_; }
  double simulationSeconds() const { return simulationMs_ / 1000.0; }
  double voltage() const;
  double tripTime() const;
  QString status() const;
  static QString modeName(Mode mode);

  void setConnected(bool connected);
  void setMode(Mode mode);
  bool enable();
  void disable();
  void emergencyStop();
  void reset();
  void advance(qint64 milliseconds);

signals:
  void changed();

private:
  bool connected_ = false;
  bool enabled_ = false;
  bool emergencyStopped_ = false;
  Mode mode_ = Mode::Teleoperated;
  qint64 elapsedMs_ = 0;
  qint64 simulationMs_ = 0;
};

} // namespace ds
