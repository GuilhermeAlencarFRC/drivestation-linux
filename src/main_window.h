#pragma once

#include "controller_hub.h"
#include "demo_session.h"
#include "network_session.h"
#include "preferences.h"
#include "robot_discovery.h"

#include <QElapsedTimer>
#include <QSet>
#include <QWidget>
#include <array>

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDialog;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QToolButton;

namespace ds {
class Battery;
class Lamp;
class TelemetryChart;
class PovIndicator;
class RumbleBar;

class MainWindow : public QWidget {
  Q_OBJECT
public:
  explicit MainWindow(PreferenceStore store = PreferenceStore(), QWidget* parent = nullptr,
                      std::unique_ptr<ControllerBackend> backend = makeSdlControllerBackend(),
                      DiscoveryTarget discoveryTarget = {});
  ~MainWindow() override;
  DemoSession& session() { return session_; }
  ControllerHub& controllers() { return controllers_; }
  NetworkSession& network() { return network_; }
  const RobotDiscovery& discovery() const { return discovery_; }
  void selectTab(int tab);
  void selectRightView(int view);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

private:
  QWidget* createOperation();
  QWidget* createDiagnostics();
  QWidget* createSetup();
  QWidget* createUsb();
  QWidget* createPower();
  QWidget* createStatus();
  QWidget* createMessages();
  QWidget* createCharts();
  QWidget* createDemoControls();
  void refresh();
  void refreshUsb();
  void stopRumble();
  void updateRumble();
  void savePreferences();
  void appendMessage(const QString& message);
  void requestEnable();
  void disableOperation();
  void emergencyStopOperation();
  void showRobotConnection();
  void refreshNetwork();
  void refreshDiscovery();
  void tick();

  PreferenceStore store_;
  Preferences preferences_;
  DemoSession session_;
  ControllerHub controllers_;
  NetworkSession network_;
  RobotDiscovery discovery_;
  bool usingNetwork_ = false;
  bool stoppingRumble_ = false;
  QElapsedTimer tickClock_;
  double lastChartSample_ = -1;
  bool settingsError_ = false;
  PracticeSequence::Phase lastPracticePhase_ = PracticeSequence::Phase::Idle;
  bool lastAutoStopped_ = false;
  QSet<int> pressedKeys_;
  QStackedWidget* pages_ = nullptr;
  std::array<QToolButton*, 5> tabButtons_{};
  std::array<QToolButton*, 3> viewButtons_{};
  std::array<QPushButton*, 4> modeButtons_{};
  QPushButton* enable_ = nullptr;
  QPushButton* disable_ = nullptr;
  QPushButton* stop_ = nullptr;
  QPushButton* autoStop_ = nullptr;
  QPushButton* reset_ = nullptr;
  QLabel* elapsed_ = nullptr;
  QLabel* elapsedCaption_ = nullptr;
  QMenu* controlsMenu_ = nullptr;
  QLabel* team_ = nullptr;
  QLabel* voltage_ = nullptr;
  QLabel* status_ = nullptr;
  QLabel* dataSource_ = nullptr;
  QLabel* footer_ = nullptr;
  Battery* battery_ = nullptr;
  std::array<Lamp*, 3> lamps_{};
  QSpinBox* teamInput_ = nullptr;
  QComboBox* dashboard_ = nullptr;
  QComboBox* station_ = nullptr;
  QLineEdit* gameData_ = nullptr;
  QCheckBox* simulate_ = nullptr;
  QDialog* connectionDialog_ = nullptr;
  QLineEdit* robotAddress_ = nullptr;
  QSpinBox* robotPort_ = nullptr;
  QSpinBox* localPort_ = nullptr;
  QCheckBox* useTcp_ = nullptr;
  QSpinBox* tcpPort_ = nullptr;
  QSpinBox* discoveryTeam_ = nullptr;
  QPushButton* findRobot_ = nullptr;
  QPushButton* cancelDiscovery_ = nullptr;
  QPushButton* useDiscovery_ = nullptr;
  QListWidget* discoveryResults_ = nullptr;
  QLabel* discoveryStatus_ = nullptr;
  QPushButton* connectRobot_ = nullptr;
  QPushButton* disconnectRobot_ = nullptr;
  QLabel* connectionStatus_ = nullptr;
  QListWidget* devices_ = nullptr;
  std::array<QProgressBar*, 6> axes_{};
  std::array<QLabel*, 12> buttons_{};
  PovIndicator* pov_ = nullptr;
  std::array<RumbleBar*, 2> rumble_{};
  QLabel* diagnostics_ = nullptr;
  QWidget* messagePanel_ = nullptr;
  QWidget* chartPanel_ = nullptr;
  QPlainTextEdit* messages_ = nullptr;
  TelemetryChart* chart_ = nullptr;
};

void applyStationStyle(QWidget& widget);

} // namespace ds
