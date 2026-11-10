#include "clock_sync.h"
#include <QTimeZone>

namespace ds {
std::optional<wire::TimeData> makeTimeData(const QDateTime& dateTime, const QByteArray& timezone,
                                           QString& error) {
  error.clear();
  const auto utc = dateTime.toUTC();
  const auto date = utc.date();
  const auto time = utc.time();
  if (!utc.isValid() || date.year() < 1900 || date.year() > 2155) {
    error = QStringLiteral("Clock is outside the UDP date/time range (1900–2155 UTC).");
    return {};
  }
  wire::TimeData result;
  result.microseconds = static_cast<quint32>(time.msec()) * 1000;
  result.second = static_cast<quint8>(time.second());
  result.minute = static_cast<quint8>(time.minute());
  result.hour = static_cast<quint8>(time.hour());
  result.day = static_cast<quint8>(date.day());
  result.month = static_cast<quint8>(date.month() - 1);
  result.yearSince1900 = static_cast<quint8>(date.year() - 1900);
  result.timezone = timezone;
  if (!wire::validTimeData(result)) {
    error = QStringLiteral("Invalid clock or unsupported system timezone identifier.");
    return {};
  }
  return result;
}

std::optional<wire::TimeData> systemTimeData(QString& error) {
  return makeTimeData(QDateTime::currentDateTimeUtc(), QTimeZone::systemTimeZoneId(), error);
}
} // namespace ds
