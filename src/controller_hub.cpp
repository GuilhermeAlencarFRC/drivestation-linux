#include "controller_hub.h"

#include <QHash>
#include <algorithm>
#include <utility>

namespace ds {

double normalizedAxis(qint16 value) { return value < 0 ? value / 32768.0 : value / 32767.0; }

ControllerHub::ControllerHub(std::unique_ptr<ControllerBackend> backend, QObject* parent)
    : QObject(parent), backend_(std::move(backend)), timer_(this) {
  timer_.setInterval(20);
  timer_.setObjectName(QStringLiteral("controllerTicker"));
  connect(&timer_, &QTimer::timeout, this, &ControllerHub::poll);
}

ControllerHub::~ControllerHub() {
  timer_.stop();
  stopRumble();
}

void ControllerHub::reportError(const QString& error) {
  if (!error.isEmpty() && error != lastError_) {
    lastError_ = error;
    emit errorOccurred(error);
  }
}

void ControllerHub::start(bool automaticPolling) {
  if (!initialized_) {
    QString error;
    initialized_ = backend_ && backend_->initialize(error);
    if (!initialized_) {
      reportError(error.isEmpty() ? QStringLiteral("Controller backend unavailable.") : error);
      return;
    }
  }
  poll();
  if (automaticPolling)
    timer_.start();
}

void ControllerHub::rescan() {
  stopRumble();
  rumbleFailed_.clear();
  if (!initialized_)
    start();
  else
    poll();
}

bool ControllerHub::anyReadable() const {
  return std::any_of(slots_.begin(), slots_.end(), [](const auto& slot) {
    return slot.device.has_value() && slot.reading.has_value();
  });
}

bool ControllerHub::restoreOrder(const ControllerOrder& order) {
  if (initialized_ || polling_)
    return false;
  savedOrder_ = normalizedControllerOrder(order);
  return true;
}

bool ControllerHub::hasMissingReservations() const {
  for (size_t i = 0; i < slots_.size(); ++i)
    if (!slots_[i].device && !savedOrder_[i].isEmpty())
      return true;
  return false;
}

bool ControllerHub::swapSlots(int from, int to) {
  if (polling_ || from < 0 || to < 0 || from >= SlotCount || to >= SlotCount || from == to ||
      (!slots_[from].device && !slots_[to].device && savedOrder_[from].isEmpty() &&
       savedOrder_[to].isEmpty()))
    return false;
  polling_ = true;
  changingTopology_ = true;
  stopRumble();
  emit topologyChanging(); // Disable must see the old mapping before it changes.
  std::swap(slots_[from], slots_[to]);
  std::swap(savedOrder_[from], savedOrder_[to]);
  emit orderChanged();
  emit updated();
  changingTopology_ = false;
  polling_ = false;
  return true;
}

bool ControllerHub::forgetMissingReservations() {
  if (polling_ || !hasMissingReservations())
    return false;
  polling_ = true;
  changingTopology_ = true;
  stopRumble();
  emit topologyChanging();
  for (size_t i = 0; i < slots_.size(); ++i)
    if (!slots_[i].device)
      savedOrder_[i].clear();
  emit orderChanged();
  emit updated();
  changingTopology_ = false;
  polling_ = false;
  return true;
}

void ControllerHub::poll() {
  if (!initialized_ || polling_)
    return;
  polling_ = true;
  QString error;
  const auto discovered = backend_->devices(error);
  QVector<ControllerInfo> available;
  QHash<ControllerId, ControllerInfo> byId;
  QHash<QString, int> identityCounts;
  if (error.isEmpty())
    for (auto device : discovered) {
      if (device.id < 0 || byId.contains(device.id))
        continue;
      if (!validControllerIdentityKey(device.persistentKey))
        device.persistentKey.clear();
      if (!device.persistentKey.isEmpty())
        ++identityCounts[device.persistentKey];
      byId.insert(device.id, device);
      available.append(device);
    }
  if (!error.isEmpty())
    available.clear();
  reportError(error);
  if (error.isEmpty())
    lastError_.clear();
  QSet<ControllerId> connected;
  for (const auto& device : available)
    if (device.id >= 0)
      connected.insert(device.id);
  auto nextSlots = slots_;
  auto nextOrder = savedOrder_;
  for (auto& key : nextOrder)
    if (identityCounts.value(key) > 1)
      key.clear(); // Ambiguous identities cannot restore or reserve a slot.
  QSet<ControllerId> assigned;
  for (int i = 0; i < SlotCount; ++i) {
    auto& slot = nextSlots[i];
    if (!slot.device)
      continue;
    const auto it = byId.constFind(slot.device->id);
    if (it == byId.cend() || (!nextOrder[i].isEmpty() && it->persistentKey != nextOrder[i])) {
      slot = {};
    } else {
      slot.device = it.value();
      assigned.insert(it->id);
    }
  }
  // Restore only a unique reported identity, leaving absent reservations empty.
  for (const auto& device : available) {
    if (assigned.contains(device.id) || device.persistentKey.isEmpty() ||
        identityCounts.value(device.persistentKey) != 1)
      continue;
    for (int i = 0; i < SlotCount; ++i)
      if (!nextSlots[i].device && nextOrder[i] == device.persistentKey) {
        nextSlots[i].device = device;
        assigned.insert(device.id);
        break;
      }
  }
  for (const auto& device : available) {
    if (assigned.contains(device.id))
      continue;
    for (int i = 0; i < SlotCount; ++i)
      if (!nextSlots[i].device && nextOrder[i].isEmpty()) {
        nextSlots[i].device = device;
        assigned.insert(device.id);
        break;
      }
  }
  for (int i = 0; i < SlotCount; ++i) {
    const auto& slot = nextSlots[i];
    if (slot.device && nextOrder[i].isEmpty() && !slot.device->persistentKey.isEmpty() &&
        identityCounts.value(slot.device->persistentKey) == 1 &&
        std::find(nextOrder.begin(), nextOrder.end(), slot.device->persistentKey) ==
            nextOrder.end())
      nextOrder[i] = slot.device->persistentKey;
  }
  bool topologyChanged = connected != knownIds_;
  for (int i = 0; i < SlotCount; ++i)
    topologyChanged |= slots_[i].device.has_value() != nextSlots[i].device.has_value() ||
                       (slots_[i].device && nextSlots[i].device &&
                        (slots_[i].device->id != nextSlots[i].device->id ||
                         slots_[i].device->persistentKey != nextSlots[i].device->persistentKey));
  if (topologyChanged) {
    changingTopology_ = true;
    stopRumble();
    emit topologyChanging();
  }
  const bool orderChanged = nextOrder != savedOrder_;
  slots_ = std::move(nextSlots);
  savedOrder_ = std::move(nextOrder);
  knownIds_ = connected;
  rumbleFailed_.intersect(connected);
  unassignedCount_ = std::max(0, static_cast<int>(connected.size() - assigned.size()));
  backend_->retain(assigned);
  for (auto& slot : slots_) {
    if (!slot.device)
      continue;
    QString readError;
    const bool wasReadable = slot.reading.has_value();
    slot.reading = backend_->read(slot.device->id, readError);
    if (readError != slot.error && !readError.isEmpty())
      emit errorOccurred(readError);
    slot.error = readError;
    if (slot.reading && rumbleFailed_.contains(slot.device->id))
      slot.reading->supportsRumble = false;
    if (wasReadable && !slot.reading) {
      changingTopology_ = true;
      stopRumble();
      emit topologyChanging();
    }
  }
  if (orderChanged)
    emit this->orderChanged();
  emit updated();
  changingTopology_ = false;
  polling_ = false;
}

bool ControllerHub::rumble(int slot, quint16 left, quint16 right) {
  if (slot < 0 || slot >= SlotCount || !slots_[slot].device || !slots_[slot].reading ||
      !slots_[slot].reading->supportsRumble)
    return false;
  const auto id = slots_[slot].device->id;
  if (rumbling_ && *rumbling_ != id)
    stopRumble();
  QString error;
  if (!backend_->rumble(id, left, right, RumblePulseMs, error)) {
    // A failed command may have reached the device. Attempt a stop even on first failure.
    rumbling_ = id;
    stopRumble();
    rumbleFailed_.insert(id);
    slots_[slot].reading->supportsRumble = false;
    reportError(error);
    emit updated();
    return false;
  }
  if (left || right)
    rumbling_ = id;
  else
    rumbling_.reset();
  return true;
}

void ControllerHub::stopRumble() {
  if (!rumbling_ || !backend_)
    return;
  const auto id = *rumbling_;
  rumbling_.reset();
  QString ignored;
  backend_->rumble(id, 0, 0, 0, ignored);
}

} // namespace ds
