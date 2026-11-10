#pragma once

#include <QByteArray>
#include <QString>
#include <array>

namespace ds {
constexpr int ControllerSlotCount = 6;
using ControllerOrder = std::array<QString, ControllerSlotCount>;

// GUID identifies a device family; serial is required to identify a unit.
QString controllerIdentityKey(const QByteArray& guid, const QByteArray& serial);
bool validControllerIdentityKey(const QString& key);
ControllerOrder normalizedControllerOrder(const ControllerOrder& order);
} // namespace ds
