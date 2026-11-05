#include "preferences.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>
#include <utility>

namespace ds {
namespace {
int boundedValue(QSettings& settings, const QString& key, int fallback, int maximum) {
  bool valid = false;
  const int value = settings.value(key, fallback).toInt(&valid);
  return valid && value >= 0 && value <= maximum ? value : fallback;
}
} // namespace

PreferenceStore::PreferenceStore(QString fileName) : fileName_(std::move(fileName)) {
  if (fileName_.isEmpty()) {
    fileName_ = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
                QStringLiteral("/settings.ini");
  }
}

Preferences PreferenceStore::load() const {
  QSettings settings(fileName_, QSettings::IniFormat);
  Preferences result;
  result.team = boundedValue(settings, QStringLiteral("team"), 0, 65535);
  result.dashboard = boundedValue(settings, QStringLiteral("dashboard"), 0, 4);
  result.station = boundedValue(settings, QStringLiteral("station"), 0, 5);
  result.gameData = settings.value(QStringLiteral("gameData")).toString().left(256);
  for (size_t i = 0; i < result.practiceSeconds.size(); ++i) {
    result.practiceSeconds[i] = boundedValue(settings, QStringLiteral("practice/%1").arg(i),
                                             result.practiceSeconds[i], 600);
  }
  return result;
}

bool PreferenceStore::save(const Preferences& preferences) const {
  QSettings settings(fileName_, QSettings::IniFormat);
  settings.setValue(QStringLiteral("team"), std::clamp(preferences.team, 0, 65535));
  settings.setValue(QStringLiteral("dashboard"), std::clamp(preferences.dashboard, 0, 4));
  settings.setValue(QStringLiteral("station"), std::clamp(preferences.station, 0, 5));
  settings.setValue(QStringLiteral("gameData"), preferences.gameData.left(256));
  for (size_t i = 0; i < preferences.practiceSeconds.size(); ++i) {
    settings.setValue(QStringLiteral("practice/%1").arg(i),
                      std::clamp(preferences.practiceSeconds[i], 0, 600));
  }
  settings.sync();
  return settings.status() == QSettings::NoError;
}

} // namespace ds
