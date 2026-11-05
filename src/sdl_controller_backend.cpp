#include "controller_backend.h"

#include <QHash>
#include <SDL.h>
#include <algorithm>

namespace ds {
namespace {
constexpr int MaximumInputs = 256;

int hatDegrees(Uint8 hat) {
  switch (hat) {
  case SDL_HAT_UP:
    return 0;
  case SDL_HAT_RIGHTUP:
    return 45;
  case SDL_HAT_RIGHT:
    return 90;
  case SDL_HAT_RIGHTDOWN:
    return 135;
  case SDL_HAT_DOWN:
    return 180;
  case SDL_HAT_LEFTDOWN:
    return 225;
  case SDL_HAT_LEFT:
    return 270;
  case SDL_HAT_LEFTUP:
    return 315;
  default:
    return -1;
  }
}

class SdlControllerBackend final : public ControllerBackend {
public:
  ~SdlControllerBackend() override {
    retain({});
    if (initialized_) {
      SDL_JoystickEventState(previousEventState_);
      SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    }
  }

  bool initialize(QString& error) override {
    error.clear();
    if (initialized_)
      return true;
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
      error = QString::fromUtf8(SDL_GetError());
      return false;
    }
    initialized_ = true;
    // Qt owns the event loop; we poll snapshots and do not consume SDL events.
    previousEventState_ = SDL_JoystickEventState(SDL_QUERY);
    SDL_JoystickEventState(SDL_IGNORE);
    return true;
  }

  QVector<ControllerInfo> devices(QString& error) override {
    error.clear();
    SDL_JoystickUpdate();
    QVector<ControllerInfo> result;
    indices_.clear();
    const int count = SDL_NumJoysticks();
    if (count < 0)
      error = QString::fromUtf8(SDL_GetError());
    for (int i = 0; i < count; ++i) {
      const auto id = SDL_JoystickGetDeviceInstanceID(i);
      if (id < 0)
        continue;
      const auto* name = SDL_JoystickNameForIndex(i);
      result.append({id, name ? QString::fromUtf8(name) : QStringLiteral("Unnamed controller")});
      indices_.insert(id, i);
    }
    return result;
  }

  std::optional<ControllerReading> read(ControllerId id, QString& error) override {
    error.clear();
    auto* joystick = handles_.value(id, nullptr);
    if (!joystick) {
      if (!indices_.contains(id)) {
        error = QStringLiteral("Controller disconnected.");
        return {};
      }
      const auto now = SDL_GetTicks64();
      if (failures_.contains(id) && now - failures_[id].time < 1000) {
        error = failures_[id].error;
        return {};
      }
      joystick = SDL_JoystickOpen(indices_[id]);
      if (!joystick) {
        error = QString::fromUtf8(SDL_GetError());
        failures_.insert(id, {now, error});
        return {};
      }
      handles_.insert(id, joystick);
      failures_.remove(id);
      SDL_JoystickUpdate();
    }
    if (!SDL_JoystickGetAttached(joystick)) {
      error = QStringLiteral("Controller disconnected.");
      return {};
    }
    ControllerReading result;
    const int axisCount = SDL_JoystickNumAxes(joystick);
    const int buttonCount = SDL_JoystickNumButtons(joystick);
    const int hatCount = SDL_JoystickNumHats(joystick);
    if (axisCount < 0 || buttonCount < 0 || hatCount < 0) {
      error = QString::fromUtf8(SDL_GetError());
      return {};
    }
    const int axes = std::min(axisCount, MaximumInputs);
    const int buttons = std::min(buttonCount, MaximumInputs);
    const int hats = std::min(hatCount, MaximumInputs);
    for (int i = 0; i < axes; ++i)
      result.axes.append(SDL_JoystickGetAxis(joystick, i));
    for (int i = 0; i < buttons; ++i)
      result.buttons.append(SDL_JoystickGetButton(joystick, i) != 0);
    for (int i = 0; i < hats; ++i)
      result.povs.append(hatDegrees(SDL_JoystickGetHat(joystick, i)));
    result.supportsRumble = SDL_JoystickHasRumble(joystick) == SDL_TRUE;
    return result;
  }

  void retain(const QSet<ControllerId>& ids) override {
    for (auto it = handles_.begin(); it != handles_.end();) {
      if (ids.contains(it.key())) {
        ++it;
        continue;
      }
      SDL_JoystickRumble(it.value(), 0, 0, 0);
      SDL_JoystickClose(it.value());
      it = handles_.erase(it);
    }
    for (auto it = failures_.begin(); it != failures_.end();) {
      if (ids.contains(it.key()))
        ++it;
      else
        it = failures_.erase(it);
    }
  }

  bool rumble(ControllerId id, quint16 left, quint16 right, quint32 milliseconds,
              QString& error) override {
    error.clear();
    auto* joystick = handles_.value(id, nullptr);
    if (!joystick || !SDL_JoystickGetAttached(joystick)) {
      error = QStringLiteral("Controller unavailable for rumble.");
      return false;
    }
    if (SDL_JoystickRumble(joystick, left, right, milliseconds) != 0) {
      error = QString::fromUtf8(SDL_GetError());
      return false;
    }
    return true;
  }

private:
  struct OpenFailure {
    Uint64 time;
    QString error;
  };
  bool initialized_ = false;
  int previousEventState_ = SDL_ENABLE;
  QHash<ControllerId, int> indices_;
  QHash<ControllerId, SDL_Joystick*> handles_;
  QHash<ControllerId, OpenFailure> failures_;
};
} // namespace

std::unique_ptr<ControllerBackend> makeSdlControllerBackend() {
  return std::make_unique<SdlControllerBackend>();
}

} // namespace ds
