#pragma once

#include "practice_sequence.h"
#include "controller_identity.h"

#include <QString>
#include <array>

namespace ds {

struct Preferences {
  int team = 0;
  int dashboard = 0;
  int station = 0;
  QString gameData;
  ControllerOrder controllerOrder{};
  PracticeSequence::Timings practiceSeconds = PracticeSequence::DefaultSeconds;
};

class PreferenceStore {
public:
  explicit PreferenceStore(QString fileName = {});
  Preferences load() const;
  bool save(const Preferences& preferences) const;
  QString fileName() const { return fileName_; }

private:
  QString fileName_;
};

} // namespace ds
