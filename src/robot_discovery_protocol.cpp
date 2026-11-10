#include "robot_discovery_protocol.h"
#include <QtEndian>

namespace ds::discovery {
namespace {
void word(QByteArray& packet, quint16 value) {
  packet.append(char(value >> 8));
  packet.append(char(value & 0xff));
}

QByteArray hostnameLabels(int team) {
  const auto hostname = robotHostname(team).toLatin1();
  if (hostname.isEmpty())
    return {};
  QByteArray result;
  for (const auto& label : hostname.split('.')) {
    result.append(char(label.size()));
    result.append(label);
  }
  return result;
}

class Reader {
public:
  explicit Reader(const QByteArray& packet) : packet_(packet) {}
  int remaining() const { return int(packet_.size()) - position_; }
  quint16 word() {
    const auto high = static_cast<quint8>(packet_[position_++]);
    const auto low = static_cast<quint8>(packet_[position_++]);
    return (quint16(high) << 8) | low;
  }
  quint32 dword() {
    const auto high = word();
    return (quint32(high) << 16) | word();
  }
  void skip(int count) { position_ += count; }
  const char* data() const { return packet_.constData() + position_; }
  std::optional<QByteArray> name() {
    int cursor = position_;
    bool jumped = false;
    int jumps = 0;
    QByteArray value;
    for (int steps = 0; steps < 256; ++steps) {
      if (cursor >= packet_.size())
        return {};
      const auto length = static_cast<quint8>(packet_[cursor]);
      if ((length & 0xc0) == 0xc0) {
        if (cursor + 1 >= packet_.size() || ++jumps > 16)
          return {};
        const int target = ((length & 0x3f) << 8) | static_cast<quint8>(packet_[cursor + 1]);
        if (target >= cursor)
          return {}; // Compression refers to a prior name occurrence.
        if (!jumped)
          position_ = cursor + 2;
        jumped = true;
        cursor = target;
        continue;
      }
      if (length & 0xc0)
        return {};
      ++cursor;
      if (length == 0) {
        if (!jumped)
          position_ = cursor;
        return value.toLower();
      }
      if (cursor + length > packet_.size())
        return {};
      // Preserve label boundaries: a dot inside a label is not a DNS separator.
      value.append(char(length));
      value.append(packet_.constData() + cursor, length);
      if (value.size() > 254) // The final root label occupies the 255th wire byte.
        return {};
      cursor += length;
      if (!jumped)
        position_ = cursor;
    }
    return {};
  }

private:
  const QByteArray& packet_;
  int position_ = 0;
};
} // namespace

QString robotHostname(int team) {
  return team > 0 && team <= 65535 ? QStringLiteral("roborio-%1-frc.local").arg(team) : QString();
}

std::optional<QHostAddress> teamAddress(int team) {
  if (team < 1 || team > 25599)
    return {};
  return QHostAddress(QStringLiteral("10.%1.%2.2").arg(team / 100).arg(team % 100));
}

bool usableIpv4(const QHostAddress& address) {
  return address.protocol() == QAbstractSocket::IPv4Protocol && !address.isNull() &&
         !address.isMulticast() && address != QHostAddress::AnyIPv4 &&
         address != QHostAddress::Broadcast;
}

std::optional<QByteArray> query(int team, quint16 id, QString& error) {
  error.clear();
  const auto hostname = hostnameLabels(team);
  if (hostname.isEmpty() || id == 0) {
    error = QStringLiteral("Set a team number from 1 to 65535 before discovery.");
    return {};
  }
  QByteArray packet;
  for (const auto field : {id, quint16(0), quint16(1), quint16(0), quint16(0), quint16(0)})
    word(packet, field);
  packet.append(hostname);
  packet.append('\0');
  word(packet, 1); // A.
  word(packet, 1); // IN; ephemeral source port requests legacy unicast replies.
  return packet;
}

std::optional<QVector<AddressRecord>> parseResponse(const QByteArray& packet, int team, quint16 id,
                                                    QString& error) {
  error.clear();
  const auto invalid = [&]() -> std::optional<QVector<AddressRecord>> {
    error = QStringLiteral("Invalid or unrelated mDNS legacy response.");
    return {};
  };
  const auto hostname = hostnameLabels(team);
  if (packet.size() < 12 || packet.size() > MaxDatagram || hostname.isEmpty() || id == 0)
    return invalid();
  Reader reader(packet);
  if (reader.word() != id)
    return invalid();
  const auto flags = reader.word();
  if ((flags & 0xf80f) != 0x8000 || (flags & 0x0200))
    return invalid();
  if (reader.word() != 1)
    return invalid();
  const auto answers = reader.word();
  const auto authority = reader.word();
  const auto additional = reader.word();
  const int total = int(answers) + authority + additional;
  if (total > MaxRecords)
    return invalid();
  const auto question = reader.name();
  if (!question || *question != hostname || reader.remaining() < 4)
    return invalid();
  if (reader.word() != 1 || reader.word() != 1)
    return invalid();
  QVector<AddressRecord> result;
  for (int record = 0; record < total; ++record) {
    const auto owner = reader.name();
    if (!owner || reader.remaining() < 10)
      return invalid();
    const auto type = reader.word();
    const auto recordClass = reader.word();
    const auto ttl = reader.dword();
    const auto length = reader.word();
    if (reader.remaining() < length)
      return invalid();
    if (type == 1 && (recordClass & 0x7fff) == 1) {
      if (length != 4)
        return invalid();
      const QHostAddress address(qFromBigEndian<quint32>(reader.data()));
      if (*owner == hostname && usableIpv4(address))
        result.append({address, ttl});
    }
    reader.skip(length);
  }
  if (reader.remaining() != 0)
    return invalid();
  return result;
}
} // namespace ds::discovery
