#include "controller_identity.h"
#include <QCryptographicHash>
#include <algorithm>

namespace ds {
QString controllerIdentityKey(const QByteArray& guid, const QByteArray& serial) {
  if (guid.size() != 16 || std::all_of(guid.begin(), guid.end(), [](char c) { return c == 0; }) ||
      serial.isEmpty() || serial.size() > 256 || serial.contains('\0'))
    return {};
  const auto data = QByteArray("drivestation-linux/controller/v1/") + guid + serial;
  return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

bool validControllerIdentityKey(const QString& key) {
  return key.size() == 64 && std::all_of(key.begin(), key.end(), [](QChar c) {
           return (c >= QLatin1Char('0') && c <= QLatin1Char('9')) ||
                  (c >= QLatin1Char('a') && c <= QLatin1Char('f'));
         });
}

ControllerOrder normalizedControllerOrder(const ControllerOrder& order) {
  ControllerOrder result{};
  for (size_t i = 0; i < order.size(); ++i)
    if (validControllerIdentityKey(order[i]) &&
        std::count(order.begin(), order.end(), order[i]) == 1)
      result[i] = order[i];
  return result;
}
} // namespace ds
