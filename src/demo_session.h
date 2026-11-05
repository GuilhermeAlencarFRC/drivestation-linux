#pragma once

#include "practice_sequence.h"

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
  bool enabled() const;
  bool busy() const;
  bool canEnable() const;
  bool emergencyStopped() const { return emergencyStopped_; }
  Mode mode() const { return mode_; }
  qint64 elapsedMs() const;
  const PracticeSequence& practice() const { return practice_; }
  double simulationSeconds() const { return simulationMs_ / 1000.0; }
  double voltage() const;
  double tripTime() const;
  QString status() const;
  static QString modeName(Mode mode);
  static QString phaseName(PracticeSequence::Phase phase);

  static constexpr qint64 MaxTickGapMs = 1000;

  void setConnected(bool connected);
  void setMode(Mode mode);
  bool setPracticeTimings(const PracticeSequence::Timings& seconds);
  bool enable();
  void disable();
  void emergencyStop();
  bool autonomousStop();
  void reset();
  void advance(qint64 milliseconds);

signals:
  void changed();

private:
  bool connected_ = false;
  bool enabled_ = false;
  bool emergencyStopped_ = false;
  Mode mode_ = Mode::Teleoperated;
  PracticeSequence practice_;
  PracticeSequence::Timings practiceSeconds_ = PracticeSequence::DefaultSeconds;
  qint64 elapsedMs_ = 0;
  qint64 simulationMs_ = 0;
};

} // namespace ds
