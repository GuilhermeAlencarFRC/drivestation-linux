#pragma once

#include "controller_backend.h"

#include <QObject>
#include <QTimer>
#include <array>

namespace ds {

struct ControllerSlot {
  std::optional<ControllerInfo> device;
  std::optional<ControllerReading> reading;
  QString error;
};

class ControllerHub : public QObject {
  Q_OBJECT
public:
  static constexpr int SlotCount = 6;
  static constexpr quint32 RumblePulseMs = 100;
  explicit ControllerHub(std::unique_ptr<ControllerBackend> backend, QObject* parent = nullptr);
  ~ControllerHub() override;
  void start(bool automaticPolling = true);
  void poll();
  void rescan();
  const std::array<ControllerSlot, SlotCount>& controllerSlots() const { return slots_; }
  bool anyReadable() const;
  int unassignedCount() const { return unassignedCount_; }
  bool rumble(int slot, quint16 left, quint16 right);
  void stopRumble();

signals:
  void topologyChanging();
  void updated();
  void errorOccurred(const QString& error);

private:
  void reportError(const QString& error);
  std::unique_ptr<ControllerBackend> backend_;
  std::array<ControllerSlot, SlotCount> slots_{};
  QTimer timer_;
  bool initialized_ = false;
  bool polling_ = false;
  int unassignedCount_ = 0;
  std::optional<ControllerId> rumbling_;
  QString lastError_;
  QSet<ControllerId> knownIds_;
  QSet<ControllerId> rumbleFailed_;
};

} // namespace ds
