#include "controller_hub.h"
#include "main_window.h"
#include "widgets.h"

#include <QLabel>
#include <QAction>
#include <QDir>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QTemporaryDir>
#include <QSettings>
#include <QToolButton>
#include <QtTest>
#include <SDL.h>
#include <algorithm>
#include <limits>

namespace {
struct RumbleCommand {
  ds::ControllerId id;
  quint16 left;
  quint16 right;
  quint32 milliseconds;
};

class FakeBackend final : public ds::ControllerBackend {
public:
  bool ready = true;
  bool failRumble = false;
  QString discoveryError;
  QVector<ds::ControllerInfo> connected;
  QHash<ds::ControllerId, ds::ControllerReading> readings;
  QSet<ds::ControllerId> retained;
  QVector<RumbleCommand> commands;
  int scans = 0;
  bool initialize(QString& error) override {
    error = ready ? QString() : QStringLiteral("Initialization failed");
    return ready;
  }
  QVector<ds::ControllerInfo> devices(QString& error) override {
    ++scans;
    error = discoveryError;
    return connected;
  }
  std::optional<ds::ControllerReading> read(ds::ControllerId id, QString& error) override {
    error = readings.contains(id) ? QString() : QStringLiteral("Read failed");
    return readings.contains(id) ? std::optional<ds::ControllerReading>(readings[id])
                                 : std::nullopt;
  }
  void retain(const QSet<ds::ControllerId>& ids) override { retained = ids; }
  bool rumble(ds::ControllerId id, quint16 left, quint16 right, quint32 ms,
              QString& error) override {
    commands.append({id, left, right, ms});
    error = failRumble ? QStringLiteral("Rumble failed") : QString();
    return !failRumble;
  }
  void add(int id, QString name = QStringLiteral("Test controller"), QString key = {}) {
    connected.append({id, std::move(name), std::move(key)});
    readings.insert(id, {{-32768, 0, 32767}, {true, false}, {45}, true});
  }
};

// Own an SDL subsystem reference and one rootless virtual device, including on assertion failure.
class VirtualController {
public:
  VirtualController() {
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0)
      return;
    initialized = true;
    SDL_VirtualJoystickDesc desc{};
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = 8;
    desc.nbuttons = 16;
    desc.nhats = 2;
    desc.name = "DriveStation SDL integration test";
    desc.userdata = this;
    desc.Rumble = [](void* data, Uint16 low, Uint16 high) -> int {
      auto* self = static_cast<VirtualController*>(data);
      self->rumbleCalls.append({0, low, high, 0});
      return self->callbackSuccess;
    };
    const int index = SDL_JoystickAttachVirtualEx(&desc);
    if (index < 0)
      return;
    id = SDL_JoystickGetDeviceInstanceID(index);
    handle = SDL_JoystickOpen(index);
    // SDL2-compat 2.32.74 forwards this SDL2 int callback directly to an SDL3 bool.
    // Calibrate only our virtual fixture; the production backend keeps SDL2 return semantics.
    // Native SDL2 accepts the documented zero and never takes this branch.
    if (handle) {
      SDL_ClearError();
      const int result = SDL_JoystickRumble(handle, 1, 1, 0);
      if (result < 0 && !rumbleCalls.isEmpty() && QString::fromUtf8(SDL_GetError()).isEmpty()) {
        callbackSuccess = 1;
        qInfo("Virtual fixture: adapting SDL2-compat callback success convention.");
      }
      SDL_JoystickRumble(handle, 0, 0, 0);
      rumbleCalls.clear();
    }
  }
  ~VirtualController() {
    if (handle)
      SDL_JoystickClose(handle);
    detach();
    if (initialized)
      SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
  }
  bool detach() {
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
      if (SDL_JoystickGetDeviceInstanceID(i) == id)
        return SDL_JoystickDetachVirtual(i) == 0;
    }
    return true;
  }
  int callbackSuccess = 0;
  bool initialized = false;
  SDL_Joystick* handle = nullptr;
  SDL_JoystickID id = -1;
  QVector<RumbleCommand> rumbleCalls;
};
} // namespace

class ControllerTest : public QObject {
  Q_OBJECT
private slots:
  void persistentIdentityRequiresSerialAndValidGuid() {
    const QByteArray guid(16, 'A');
    const auto key = ds::controllerIdentityKey(guid, "unit-a");
    QVERIFY(ds::validControllerIdentityKey(key));
    QCOMPARE(key, ds::controllerIdentityKey(guid, "unit-a"));
    QVERIFY(key != ds::controllerIdentityKey(guid, "unit-b"));
    QVERIFY(key != ds::controllerIdentityKey(QByteArray(16, 'B'), "unit-a"));
    QVERIFY(ds::controllerIdentityKey(guid, {}).isEmpty());
    QVERIFY(ds::controllerIdentityKey(QByteArray(16, '\0'), "unit-a").isEmpty());
    QVERIFY(ds::controllerIdentityKey(QByteArray(15, 'A'), "unit-a").isEmpty());
    QVERIFY(ds::controllerIdentityKey(guid, QByteArray(257, 'x')).isEmpty());
    QVERIFY(ds::controllerIdentityKey(guid, QByteArray("a\0b", 3)).isEmpty());
  }

  void savedOrderRestoresAcrossNewInstanceIdsAndEnumerationOrder() {
    const auto a = ds::controllerIdentityKey(QByteArray(16, 'A'), "a");
    const auto b = ds::controllerIdentityKey(QByteArray(16, 'A'), "b");
    auto backend = std::make_unique<FakeBackend>();
    auto* fake = backend.get();
    fake->add(1, "A", a);
    fake->add(2, "B", b);
    ds::ControllerHub hub(std::move(backend));
    hub.start(false);
    QVERIFY(!hub.restoreOrder({}));
    QVERIFY(hub.rumble(0, 100, 200));
    QSignalSpy changed(&hub, &ds::ControllerHub::topologyChanging);
    connect(&hub, &ds::ControllerHub::topologyChanging, &hub, [&] {
      QCOMPARE(hub.controllerSlots()[0].device->id, 1); // Still the old mapping.
      QVERIFY(!hub.swapSlots(0, 1)); // Reentrant mutation is rejected.
    });
    QVERIFY(hub.swapSlots(0, 1));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(fake->commands.last().left, quint16(0));
    QCOMPARE(hub.controllerSlots()[0].device->id, 2);
    hub.poll();
    QCOMPARE(changed.count(), 1);
    const auto order = hub.savedOrder();
    QCOMPARE(order[0], b);
    QCOMPARE(order[1], a);
    auto restarted = std::make_unique<FakeBackend>();
    restarted->add(101, "A", a); // Different IDs, A still enumerated before B.
    restarted->add(202, "B", b);
    ds::ControllerHub restored(std::move(restarted));
    QVERIFY(restored.restoreOrder(order));
    restored.start(false);
    QCOMPARE(restored.controllerSlots()[0].device->id, 202);
    QCOMPARE(restored.controllerSlots()[1].device->id, 101);
    QVERIFY(!restored.swapSlots(-1, 0));
    QVERIFY(!restored.swapSlots(0, 6));
    QVERIFY(!restored.swapSlots(0, 0));
  }

  void missingReservationsDoNotSilentlyBindAnotherController() {
    const auto a = ds::controllerIdentityKey(QByteArray(16, 'A'), "a");
    const auto b = ds::controllerIdentityKey(QByteArray(16, 'A'), "b");
    ds::ControllerOrder order{};
    order[4] = a;
    order[2] = b;
    auto backend = std::make_unique<FakeBackend>();
    backend->add(11, "A", a);
    for (int i = 20; i < 28; ++i)
      backend->add(i); // No persistent identity.
    ds::ControllerHub hub(std::move(backend));
    hub.restoreOrder(order);
    hub.start(false);
    QCOMPARE(hub.controllerSlots()[4].device->id, 11);
    QVERIFY(!hub.controllerSlots()[2].device);
    QVERIFY(hub.hasMissingReservations());
    QCOMPARE(hub.unassignedCount(), 4);
    QSignalSpy changed(&hub, &ds::ControllerHub::topologyChanging);
    QVERIFY(hub.forgetMissingReservations());
    QCOMPARE(changed.count(), 1);
    hub.poll(); // A waiting device fills the freed slot without a USB-list change.
    QCOMPARE(changed.count(), 2);
    QVERIFY(hub.controllerSlots()[2].device);
    QCOMPARE(hub.unassignedCount(), 3);
    QVERIFY(!hub.hasMissingReservations());
  }

  void duplicateIdentitiesAndInvalidPreferencesAreNotRestored() {
    const auto key = ds::controllerIdentityKey(QByteArray(16, 'A'), "same");
    ds::ControllerOrder order{};
    order[5] = key;
    auto backend = std::make_unique<FakeBackend>();
    backend->add(1, "A", key);
    backend->add(2, "B", key);
    ds::ControllerHub hub(std::move(backend));
    hub.restoreOrder(order);
    hub.start(false);
    QVERIFY(!hub.controllerSlots()[5].device);
    QVERIFY(std::all_of(hub.savedOrder().begin(), hub.savedOrder().end(),
                         [](const auto& value) { return value.isEmpty(); }));
    QTemporaryDir directory;
    const auto path = directory.filePath("preferences.ini");
    ds::Preferences preferences;
    preferences.controllerOrder[3] = key;
    ds::PreferenceStore store(path);
    QVERIFY(store.save(preferences));
    QCOMPARE(store.load().controllerOrder[3], key);
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue("controllers/slot0", key);
    settings.setValue("controllers/slot1", "invalid-key");
    settings.sync();
    const auto loaded = store.load();
    QVERIFY(loaded.controllerOrder[0].isEmpty());
    QVERIFY(loaded.controllerOrder[1].isEmpty());
    QVERIFY(loaded.controllerOrder[3].isEmpty()); // Both duplicate reservations rejected.
  }

  void uiSwapCancelsPracticeAndPersistsTheSelectedController() {
    QTemporaryDir directory;
    const auto a = ds::controllerIdentityKey(QByteArray(16, 'A'), "a");
    const auto b = ds::controllerIdentityKey(QByteArray(16, 'A'), "b");
    const auto path = directory.filePath("order.ini");
    {
      auto backend = std::make_unique<FakeBackend>();
      backend->add(1, "A", a);
      backend->add(2, "B", b);
      ds::MainWindow window(ds::PreferenceStore(path), nullptr, std::move(backend));
      window.selectTab(3);
      window.show();
      auto* list = window.findChild<QListWidget*>(QStringLiteral("devices"));
      window.session().setConnected(true);
      window.session().setMode(ds::DemoSession::Mode::Practice);
      QVERIFY(window.session().enable());
      QVERIFY(window.session().busy());
      window.findChild<QToolButton*>(QStringLiteral("moveControllerDown"))->click();
      QVERIFY(!window.session().busy());
      QCOMPARE(list->currentRow(), 1);
      QCOMPARE(window.controllers().controllerSlots()[1].device->id, 1);
      QCOMPARE(ds::PreferenceStore(path).load().controllerOrder[1], a);
      const auto evidence = qEnvironmentVariable("DS_EVIDENCE_DIR");
      if (!evidence.isEmpty())
        QVERIFY(window.grab().save(QDir(evidence).filePath("usb-order.png")));
    }
    auto backend = std::make_unique<FakeBackend>();
    backend->add(10, "A", a);
    ds::MainWindow restored(ds::PreferenceStore(path), nullptr, std::move(backend));
    QCOMPARE(restored.controllers().controllerSlots()[1].device->id, 10);
    QVERIFY(!restored.controllers().controllerSlots()[0].device);
    auto* forget = restored.findChild<QAction*>(QStringLiteral("forgetMissingControllers"));
    QVERIFY(forget->isEnabled());
    forget->trigger();
    QVERIFY(ds::PreferenceStore(path).load().controllerOrder[0].isEmpty());
    QCOMPARE(ds::PreferenceStore(path).load().controllerOrder[1], a);
  }

  void normalizationPreservesEndpointsAndCenter() {
    QCOMPARE(ds::normalizedAxis(std::numeric_limits<qint16>::min()), -1.0);
    QCOMPARE(ds::normalizedAxis(0), 0.0);
    QCOMPARE(ds::normalizedAxis(std::numeric_limits<qint16>::max()), 1.0);
    QCOMPARE(ds::normalizedAxis(-16384), -0.5);
    QVERIFY(ds::normalizedAxis(16384) > 0.5);
  }

  void slotsStayStableAndExcessDevicesDoNotChurn() {
    auto backend = std::make_unique<FakeBackend>();
    auto* fake = backend.get();
    for (int id = 10; id < 18; ++id)
      fake->add(id);
    ds::ControllerHub hub(std::move(backend));
    QSignalSpy topology(&hub, &ds::ControllerHub::topologyChanging);
    hub.start(false);
    QCOMPARE(topology.count(), 1);
    QCOMPARE(hub.unassignedCount(), 2);
    QCOMPARE(fake->retained.size(), 6);
    QVERIFY(!fake->retained.contains(16));
    hub.poll();
    hub.poll();
    QCOMPARE(topology.count(), 1);
    fake->connected.removeAt(1); // Slot 1 disappears; slot 2 must stay put.
    hub.poll();
    QCOMPARE(topology.count(), 2);
    QCOMPARE(hub.controllerSlots()[0].device->id, 10);
    QCOMPARE(hub.controllerSlots()[1].device->id, 16);
    QCOMPARE(hub.controllerSlots()[2].device->id, 12);
    QCOMPARE(hub.unassignedCount(), 1);
    fake->connected.clear();
    hub.poll();
    QVERIFY(!hub.anyReadable());
    for (const auto& slot : hub.controllerSlots()) {
      QVERIFY(!slot.device);
      QVERIFY(!slot.reading);
    }
    QVERIFY(fake->retained.isEmpty());
    fake->add(30);
    hub.poll();
    QCOMPARE(hub.controllerSlots()[0].device->id, 30);
  }

  void failedReadClearsOldValuesAndReportsOnceUntilRecovery() {
    auto backend = std::make_unique<FakeBackend>();
    auto* fake = backend.get();
    fake->add(1);
    fake->add(2);
    ds::ControllerHub hub(std::move(backend));
    hub.start(false);
    QVERIFY(hub.anyReadable());
    QSignalSpy errors(&hub, &ds::ControllerHub::errorOccurred);
    QSignalSpy topology(&hub, &ds::ControllerHub::topologyChanging);
    QVERIFY(hub.rumble(0, 1000, 2000));
    fake->readings.clear();
    hub.poll();
    QVERIFY(!hub.anyReadable());
    QVERIFY(!hub.controllerSlots()[0].reading);
    QVERIFY(hub.controllerSlots()[0].device);
    QCOMPARE(fake->commands.last().left, quint16(0));
    QCOMPARE(fake->commands.last().right, quint16(0));
    QCOMPARE(errors.count(), 2);
    QCOMPARE(topology.count(), 2);
    hub.poll();
    QCOMPARE(errors.count(), 2);
    QCOMPARE(topology.count(), 2);
    fake->readings.insert(1, {{123}, {}, {}, false});
    hub.poll();
    QVERIFY(hub.anyReadable());
    QCOMPARE(hub.controllerSlots()[0].reading->axes[0], qint16(123));
    fake->readings.clear();
    hub.poll();
    QCOMPARE(errors.count(), 3);
  }

  void discoveryFailureClearsInputsAndReportsAgainAfterRecovery() {
    auto backend = std::make_unique<FakeBackend>();
    auto* fake = backend.get();
    fake->add(1);
    ds::ControllerHub hub(std::move(backend));
    hub.start(false);
    QSignalSpy errors(&hub, &ds::ControllerHub::errorOccurred);
    fake->discoveryError = QStringLiteral("Discovery failed");
    hub.poll();
    QVERIFY(!hub.anyReadable());
    QVERIFY(!hub.controllerSlots()[0].device);
    hub.poll();
    QCOMPARE(errors.count(), 1);
    fake->discoveryError.clear();
    hub.poll();
    QVERIFY(hub.anyReadable());
    fake->discoveryError = QStringLiteral("Discovery failed");
    hub.poll();
    QCOMPARE(errors.count(), 2);
  }

  void initializationCanBeRetriedWithoutInventingInputs() {
    auto backend = std::make_unique<FakeBackend>();
    auto* fake = backend.get();
    fake->ready = false;
    ds::ControllerHub hub(std::move(backend));
    QSignalSpy errors(&hub, &ds::ControllerHub::errorOccurred);
    hub.start(false);
    hub.start(false);
    QCOMPARE(errors.count(), 1);
    QVERIFY(!hub.anyReadable());
    fake->ready = true;
    fake->add(9);
    hub.rescan();
    QVERIFY(hub.anyReadable());
    QCOMPARE(hub.controllerSlots()[0].device->id, 9);
  }

  void rumbleRoutesMotorsAndStopsOnSelectionAndTopology() {
    auto backend = std::make_unique<FakeBackend>();
    auto* fake = backend.get();
    fake->add(11);
    fake->add(22);
    ds::ControllerHub hub(std::move(backend));
    hub.start(false);
    QVERIFY(!hub.rumble(-1, 1, 1));
    QVERIFY(!hub.rumble(6, 1, 1));
    QVERIFY(hub.rumble(0, 1234, 5678));
    QCOMPARE(fake->commands.last().id, 11);
    QCOMPARE(fake->commands.last().left, quint16(1234));
    QCOMPARE(fake->commands.last().right, quint16(5678));
    QCOMPARE(fake->commands.last().milliseconds, quint32(100));
    QVERIFY(hub.rumble(1, 0, 9000));
    QCOMPARE(fake->commands[fake->commands.size() - 2].id, 11);
    QCOMPARE(fake->commands[fake->commands.size() - 2].left, quint16(0));
    QCOMPARE(fake->commands[fake->commands.size() - 2].right, quint16(0));
    fake->connected.removeLast();
    hub.poll();
    QCOMPARE(fake->commands.last().id, 22);
    QCOMPARE(fake->commands.last().right, quint16(0));
    QCOMPARE(fake->commands.last().milliseconds, quint32(0));
  }

  void rumbleFailureStaysDisabledUntilExplicitRescan() {
    auto backend = std::make_unique<FakeBackend>();
    auto* fake = backend.get();
    fake->add(7);
    ds::ControllerHub hub(std::move(backend));
    hub.start(false);
    fake->failRumble = true;
    QVERIFY(!hub.rumble(0, 6000, 6000));
    QCOMPARE(fake->commands.last().left, quint16(0));
    hub.poll();
    QVERIFY(!hub.controllerSlots()[0].reading->supportsRumble);
    const int count = fake->commands.size();
    QVERIFY(!hub.rumble(0, 1, 1));
    QCOMPARE(fake->commands.size(), count);
    fake->failRumble = false;
    hub.rescan();
    QVERIFY(hub.rumble(0, 1, 1));
    hub.stopRumble();
    QCOMPARE(fake->commands.last().left, quint16(0));
  }

  void realSdlReadsVirtualInputsAndDetectsRemoval() {
    VirtualController device;
    QVERIFY2(device.handle, SDL_GetError());
    auto backend = ds::makeSdlControllerBackend();
    QString error;
    QVERIFY2(backend->initialize(error), qPrintable(error));
    const auto found = backend->devices(error);
    QVERIFY(std::any_of(found.begin(), found.end(),
                        [&](const auto& info) { return info.id == device.id; }));
    const auto info = std::find_if(found.begin(), found.end(),
                                   [&](const auto& value) { return value.id == device.id; });
    const auto guid = SDL_JoystickGetGUID(device.handle);
    const auto* serial = SDL_JoystickGetSerial(device.handle);
    QCOMPARE(info->persistentKey, ds::controllerIdentityKey(
                                      QByteArray(reinterpret_cast<const char*>(guid.data), 16),
                                      serial ? QByteArray(serial) : QByteArray()));
    QVERIFY(backend->read(device.id, error));
    QCOMPARE(SDL_JoystickSetVirtualAxis(device.handle, 0, -32768), 0);
    QCOMPARE(SDL_JoystickSetVirtualAxis(device.handle, 1, 0), 0);
    QCOMPARE(SDL_JoystickSetVirtualAxis(device.handle, 7, 32767), 0);
    QCOMPARE(SDL_JoystickSetVirtualButton(device.handle, 15, 1), 0);
    QCOMPARE(SDL_JoystickSetVirtualHat(device.handle, 0, SDL_HAT_LEFTUP), 0);
    QCOMPARE(SDL_JoystickSetVirtualHat(device.handle, 1, SDL_HAT_RIGHTDOWN), 0);
    backend->devices(error);
    auto reading = backend->read(device.id, error);
    QVERIFY2(reading, qPrintable(error));
    QCOMPARE(reading->axes.size(), 8);
    QCOMPARE(reading->axes[0], qint16(-32768));
    QCOMPARE(reading->axes[1], qint16(0));
    QCOMPARE(reading->axes[7], qint16(32767));
    QCOMPARE(reading->buttons.size(), 16);
    QVERIFY(reading->buttons[15]);
    QCOMPARE(reading->povs, QVector<int>({315, 135}));
    QCOMPARE(SDL_JoystickSetVirtualHat(device.handle, 0, SDL_HAT_CENTERED), 0);
    QCOMPARE(SDL_JoystickSetVirtualButton(device.handle, 15, 0), 0);
    backend->devices(error);
    reading = backend->read(device.id, error);
    QVERIFY(reading);
    QCOMPARE(reading->povs[0], -1);
    QVERIFY(!reading->buttons[15]);
    const std::array<Uint8, 8> directions{SDL_HAT_UP,        SDL_HAT_RIGHTUP, SDL_HAT_RIGHT,
                                          SDL_HAT_RIGHTDOWN, SDL_HAT_DOWN,    SDL_HAT_LEFTDOWN,
                                          SDL_HAT_LEFT,      SDL_HAT_LEFTUP};
    for (int direction = 0; direction < 8; ++direction) {
      QCOMPARE(SDL_JoystickSetVirtualHat(device.handle, 0, directions[direction]), 0);
      backend->devices(error);
      const auto updated = backend->read(device.id, error);
      QVERIFY(updated);
      QCOMPARE(updated->povs[0], direction * 45);
    }
    QVERIFY(device.detach());
    const auto removed = backend->devices(error);
    QVERIFY(std::none_of(removed.begin(), removed.end(),
                         [&](const auto& info) { return info.id == device.id; }));
    QVERIFY(!backend->read(device.id, error));
    backend->retain({});
    QVERIFY(!error.isEmpty());
  }

  void staleSdlIndexCannotReadAReplacementDevice() {
    VirtualController original;
    QVERIFY2(original.handle, SDL_GetError());
    auto backend = ds::makeSdlControllerBackend();
    QString error;
    QVERIFY(backend->initialize(error));
    backend->devices(error); // Cache the old index without retaining a read handle.
    QVERIFY(original.detach());
    VirtualController replacement;
    QVERIFY2(replacement.handle, SDL_GetError());
    QVERIFY(replacement.id != original.id);
    QVERIFY(!backend->read(original.id, error));
    QVERIFY(!error.isEmpty());
    backend->devices(error);
    QVERIFY(backend->read(replacement.id, error));
  }

  void realSdlFindsControllersAttachedAfterInitialization() {
    auto backend = ds::makeSdlControllerBackend();
    QString error;
    QVERIFY(backend->initialize(error));
    backend->devices(error);
    {
      VirtualController added;
      QVERIFY2(added.handle, SDL_GetError());
      const auto available = backend->devices(error);
      QVERIFY(std::any_of(available.begin(), available.end(),
                          [&](const auto& info) { return info.id == added.id; }));
      QVERIFY(backend->read(added.id, error));
      backend.reset();
      // Releasing our backend must not shut down the fixture's SDL subsystem reference.
      QVERIFY(SDL_WasInit(SDL_INIT_JOYSTICK));
      QVERIFY(SDL_JoystickGetAttached(added.handle));
    }
  }

  void realSdlRumbleAndReleaseUseVirtualCallback() {
    VirtualController device;
    QVERIFY2(device.handle, SDL_GetError());
    auto backend = ds::makeSdlControllerBackend();
    QString error;
    QVERIFY(backend->initialize(error));
    backend->devices(error);
    const auto reading = backend->read(device.id, error);
    QVERIFY(reading);
    QVERIFY(reading->supportsRumble);
    QVERIFY2(backend->rumble(device.id, 12345, 54321, 100, error), qPrintable(error));
    QVERIFY(!device.rumbleCalls.isEmpty());
    QCOMPARE(device.rumbleCalls.last().left, quint16(12345));
    QCOMPARE(device.rumbleCalls.last().right, quint16(54321));
    backend->retain({});
    QCOMPARE(device.rumbleCalls.last().left, quint16(0));
    QCOMPARE(device.rumbleCalls.last().right, quint16(0));
    QVERIFY(!backend->rumble(device.id, 1, 1, 100, error));
  }

  void uiShowsSelectedRealInputsAndDemoDoesNotInventUsb() {
    QTemporaryDir directory;
    auto backend = std::make_unique<FakeBackend>();
    auto* fake = backend.get();
    fake->add(1, QStringLiteral("USB A"));
    fake->add(2, QStringLiteral("USB B"));
    fake->readings[2] = {{16384}, {false}, {-1}, false};
    ds::MainWindow window(ds::PreferenceStore(directory.filePath("usb.ini")), nullptr,
                          std::move(backend));
    window.selectTab(3);
    window.show();
    QTest::qWait(10);
    auto* devices = window.findChild<QListWidget*>(QStringLiteral("devices"));
    auto* axis = window.findChild<QProgressBar*>(QStringLiteral("axis0"));
    auto* button = window.findChild<QLabel*>(QStringLiteral("button1"));
    auto* pov = dynamic_cast<ds::PovIndicator*>(window.findChild<QWidget*>(QStringLiteral("pov")));
    QVERIFY(devices && axis && button && pov);
    auto* joystickLamp = window.findChild<QWidget*>(QStringLiteral("statusLamp2"));
    auto* communicationsLamp = window.findChild<QWidget*>(QStringLiteral("statusLamp0"));
    QVERIFY(joystickLamp && communicationsLamp);
    QCOMPARE(joystickLamp->accessibleDescription(), QStringLiteral("Present"));
    QCOMPARE(communicationsLamp->accessibleDescription(), QStringLiteral("Unavailable"));
    QCOMPARE(axis->value(), 0);
    QCOMPARE(axis->text(), QStringLiteral("-1.00"));
    QCOMPARE(button->accessibleDescription(), QStringLiteral("Pressed"));
    QCOMPARE(pov->angle(), std::optional<int>(45));
    window.session().setConnected(true);
    window.session().advance(500);
    QCOMPARE(axis->text(), QStringLiteral("-1.00"));
    QVERIFY(window.session().enable());
    devices->setCurrentRow(1);
    QVERIFY(window.session().enabled());
    QCOMPARE(axis->text(), QStringLiteral("0.50"));
    QCOMPARE(button->accessibleDescription(), QStringLiteral("Released"));
    QCOMPARE(pov->angle(), std::optional<int>(-1));
    fake->connected.clear();
    window.controllers().poll();
    QCOMPARE(axis->text(), QStringLiteral("—"));
    QCOMPARE(button->accessibleDescription(), QStringLiteral("Unavailable"));
    QVERIFY(!pov->angle());
    QVERIFY(!window.controllers().anyReadable());
    QCOMPARE(joystickLamp->accessibleDescription(), QStringLiteral("Unavailable"));
    window.session().advance(500);
    QCOMPARE(devices->item(0)->text(), QStringLiteral("0  —"));
    const int scans = fake->scans;
    QTest::keyClick(&window, Qt::Key_F1);
    QVERIFY(fake->scans > scans);
  }

  void usbChangeCancelsPendingPracticeAndDoesNotResumeIt() {
    QTemporaryDir directory;
    auto backend = std::make_unique<FakeBackend>();
    auto* fake = backend.get();
    fake->add(1);
    ds::MainWindow window(ds::PreferenceStore(directory.filePath("practice-usb.ini")), nullptr,
                          std::move(backend));
    window.session().setConnected(true);
    window.session().setMode(ds::DemoSession::Mode::Practice);
    QVERIFY(window.session().enable());
    QVERIFY(window.session().busy());
    fake->connected.clear();
    window.controllers().poll();
    QVERIFY(!window.session().busy());
    fake->add(3);
    window.controllers().poll();
    window.session().advance(1000);
    QVERIFY(!window.session().busy());
    QVERIFY(!window.session().enabled());
  }

  void uiRumbleStopsAndNeverRestartsAfterCancellation_data() {
    QTest::addColumn<int>("cause");
    QTest::newRow("release") << 0;
    QTest::newRow("selection") << 1;
    QTest::newRow("tab") << 2;
    QTest::newRow("focus") << 3;
    QTest::newRow("disconnect") << 4;
    QTest::newRow("close") << 5;
    QTest::newRow("output-error") << 6;
    QTest::newRow("slot-order") << 7;
  }
  void uiRumbleStopsAndNeverRestartsAfterCancellation() {
    QFETCH(int, cause);
    QTemporaryDir directory;
    auto backend = std::make_unique<FakeBackend>();
    auto* fake = backend.get();
    fake->add(1);
    fake->add(2);
    ds::MainWindow window(ds::PreferenceStore(directory.filePath("rumble.ini")), nullptr,
                          std::move(backend));
    window.selectTab(3);
    window.show();
    QTest::qWait(10);
    auto* bar = window.findChild<ds::RumbleBar*>(QStringLiteral("rumbleRight"));
    QVERIFY(bar && bar->isEnabled());
    QTest::mousePress(bar, Qt::LeftButton, Qt::NoModifier, QPoint(bar->width() - 1, 5));
    QVERIFY(!fake->commands.isEmpty());
    QCOMPARE(fake->commands.last().left, quint16(0));
    QCOMPARE(fake->commands.last().right, quint16(65535));
    QCOMPARE(fake->commands.last().milliseconds, quint32(100));
    switch (cause) {
    case 0:
      QTest::mouseRelease(bar, Qt::LeftButton);
      break;
    case 1:
      window.findChild<QListWidget*>(QStringLiteral("devices"))->setCurrentRow(1);
      break;
    case 2:
      window.selectTab(0);
      break;
    case 3: {
      QEvent deactivate(QEvent::WindowDeactivate);
      QApplication::sendEvent(&window, &deactivate);
      break;
    }
    case 4:
      fake->connected.clear();
      window.controllers().poll();
      break;
    case 5:
      window.close();
      break;
    case 6:
      fake->failRumble = true;
      QTest::qWait(80);
      break;
    case 7:
      QVERIFY(window.controllers().swapSlots(0, 1));
      break;
    }
    QCOMPARE(bar->strength(), quint16(0));
    QCOMPARE(fake->commands.last().left, quint16(0));
    QCOMPARE(fake->commands.last().right, quint16(0));
    const int count = fake->commands.size();
    QTest::qWait(130);
    QCOMPARE(fake->commands.size(), count);
  }

  void delayedRumbleTimerCancelsInsteadOfRestarting() {
    ds::RumbleBar bar;
    bar.show();
    QTest::qWait(10);
    QTest::mousePress(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(39, 5));
    QCOMPARE(bar.strength(), quint16(65535));
    QTest::qSleep(230); // Deliberately block event delivery as a stalled GUI would.
    QApplication::processEvents();
    QCOMPARE(bar.strength(), quint16(0));
  }
};

QTEST_MAIN(ControllerTest)
#include "test_controllers.moc"
