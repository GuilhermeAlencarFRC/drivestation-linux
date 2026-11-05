#pragma once

#include <QSet>
#include <QString>
#include <QVector>
#include <memory>
#include <optional>

namespace ds {

using ControllerId = qint32;

struct ControllerInfo {
  ControllerId id = -1;
  QString name;
};

struct ControllerReading {
  QVector<qint16> axes;
  QVector<bool> buttons;
  QVector<int> povs; // Degrees clockwise from up; -1 means centered.
  bool supportsRumble = false;
};

class ControllerBackend {
public:
  virtual ~ControllerBackend() = default;
  virtual bool initialize(QString& error) = 0;
  virtual QVector<ControllerInfo> devices(QString& error) = 0;
  virtual std::optional<ControllerReading> read(ControllerId id, QString& error) = 0;
  virtual void retain(const QSet<ControllerId>& ids) = 0;
  virtual bool rumble(ControllerId id, quint16 left, quint16 right, quint32 milliseconds,
                      QString& error) = 0;
};

std::unique_ptr<ControllerBackend> makeSdlControllerBackend();
double normalizedAxis(qint16 value);

} // namespace ds
