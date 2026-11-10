#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QString>
#include <QVector>
#include <optional>

namespace ds::discovery {
constexpr quint16 Port = 5353;
constexpr int MaxDatagram = 9000;
constexpr int MaxRecords = 64;
constexpr int MaxAddresses = 16;

struct AddressRecord {
  QHostAddress address;
  quint32 ttlSeconds = 0;
};

QString robotHostname(int team);
std::optional<QHostAddress> teamAddress(int team);
bool usableIpv4(const QHostAddress& address);
std::optional<QByteArray> query(int team, quint16 id, QString& error);
// Legacy unicast response: requires the same ID and exactly one repeated A/IN question.
std::optional<QVector<AddressRecord>> parseResponse(const QByteArray& packet, int team, quint16 id,
                                                    QString& error);
} // namespace ds::discovery
