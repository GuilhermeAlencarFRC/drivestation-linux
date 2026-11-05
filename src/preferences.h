#pragma once

#include <QString>
#include <array>

namespace ds {

struct Preferences {
  int team = 0;
  int dashboard = 0;
  int station = 0;
  QString gameData;
  std::array<int, 5> practiceSeconds{5, 15, 1, 105, 30};
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
