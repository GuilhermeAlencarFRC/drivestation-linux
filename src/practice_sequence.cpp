#include "practice_sequence.h"

#include <algorithm>

namespace ds {

bool PracticeSequence::validTimings(const Timings& seconds) {
  return std::all_of(seconds.begin(), seconds.end(),
                     [](int value) { return value >= 0 && value <= 600; });
}

bool PracticeSequence::active() const {
  return phase_ >= Phase::Countdown && phase_ <= Phase::EndGame;
}

bool PracticeSequence::enabled() const {
  return (phase_ == Phase::Autonomous && !autoStopped_) || phase_ == Phase::Teleoperated ||
         phase_ == Phase::EndGame;
}

bool PracticeSequence::start(const Timings& seconds) {
  if (active() || !validTimings(seconds))
    return false;
  reset();
  for (size_t i = 0; i < seconds.size(); ++i) {
    durations_[i] = static_cast<Milliseconds>(seconds[i]) * 1000;
    totalMs_ += durations_[i];
  }
  updatePhase();
  return true;
}

void PracticeSequence::updatePhase() {
  Milliseconds boundary = 0;
  for (size_t i = 0; i < durations_.size(); ++i) {
    boundary += durations_[i];
    if (elapsedMs_ < boundary) {
      phase_ = static_cast<Phase>(i + 1);
      return;
    }
  }
  phase_ = Phase::Complete;
}

void PracticeSequence::advance(Milliseconds milliseconds) {
  if (milliseconds <= 0 || !active())
    return;
  // Clamp before addition, including for a caller passing INT64_MAX.
  elapsedMs_ += std::min(milliseconds, totalMs_ - elapsedMs_);
  updatePhase();
}

void PracticeSequence::cancel() {
  if (active())
    phase_ = Phase::Cancelled;
}

void PracticeSequence::reset() {
  durations_.fill(0);
  totalMs_ = 0;
  elapsedMs_ = 0;
  phase_ = Phase::Idle;
  autoStopped_ = false;
}

bool PracticeSequence::autonomousStop() {
  if (phase_ != Phase::Autonomous || autoStopped_)
    return false;
  autoStopped_ = true;
  return true;
}

PracticeSequence::Milliseconds PracticeSequence::remainingMs() const {
  if (!active())
    return 0;
  Milliseconds boundary = 0;
  const auto index = static_cast<size_t>(phase_) - 1;
  for (size_t i = 0; i <= index; ++i)
    boundary += durations_[i];
  return boundary - elapsedMs_;
}

bool PracticeSequence::pendingEnable() const {
  if (!active() || enabled())
    return false;
  const auto index = static_cast<size_t>(phase_) - 1;
  // Enabled segments are Autonomous, Teleoperated and End Game.
  for (size_t i = index + 1; i < durations_.size(); ++i) {
    if ((i == 1 || i == 3 || i == 4) && durations_[i] > 0)
      return true;
  }
  return false;
}

} // namespace ds
