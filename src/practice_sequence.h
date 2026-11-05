#pragma once

#include <array>
#include <cstdint>

namespace ds {

// Pure local timing model: no clock reads, widgets, devices or robot transport.
class PracticeSequence {
public:
  using Milliseconds = std::int64_t;
  using Timings = std::array<int, 5>;
  static constexpr Timings DefaultSeconds{5, 15, 1, 105, 30};
  enum class Phase {
    Idle,
    Countdown,
    Autonomous,
    Delay,
    Teleoperated,
    EndGame,
    Complete,
    Cancelled
  };

  static bool validTimings(const Timings& seconds);
  bool start(const Timings& seconds);
  void advance(Milliseconds milliseconds);
  void cancel();
  void reset();
  bool autonomousStop();

  Phase phase() const { return phase_; }
  bool active() const;
  bool enabled() const;
  bool autoStopped() const { return autoStopped_ && phase_ == Phase::Autonomous; }
  bool pendingEnable() const;
  Milliseconds elapsedMs() const { return elapsedMs_; }
  Milliseconds remainingMs() const;

private:
  void updatePhase();
  std::array<Milliseconds, 5> durations_{};
  Milliseconds totalMs_ = 0;
  Milliseconds elapsedMs_ = 0;
  Phase phase_ = Phase::Idle;
  bool autoStopped_ = false;
};

} // namespace ds
