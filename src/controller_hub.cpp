#include "controller_hub.h"

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

void ControllerHub::poll() {
  if (!initialized_ || polling_)
    return;
  polling_ = true;
  QString error;
  auto available = backend_->devices(error);
  if (!error.isEmpty())
    available.clear();
  reportError(error);
  if (error.isEmpty())
    lastError_.clear();
  QSet<ControllerId> connected;
  for (const auto& device : available)
    if (device.id >= 0)
      connected.insert(device.id);
  QSet<ControllerId> assigned;
  for (const auto& slot : slots_) {
    if (slot.device && connected.contains(slot.device->id))
      assigned.insert(slot.device->id);
  }
  if (connected != knownIds_) {
    stopRumble();
    emit topologyChanging();
  }
  knownIds_ = connected;
  rumbleFailed_.intersect(connected);
  for (auto& slot : slots_) {
    if (slot.device && !connected.contains(slot.device->id))
      slot = {};
  }
  for (const auto& device : available) {
    if (device.id < 0 || assigned.contains(device.id))
      continue;
    auto empty =
        std::find_if(slots_.begin(), slots_.end(), [](const auto& slot) { return !slot.device; });
    if (empty == slots_.end())
      break;
    empty->device = device;
    assigned.insert(device.id);
  }
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
      stopRumble();
      emit topologyChanging();
    }
  }
  emit updated();
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
