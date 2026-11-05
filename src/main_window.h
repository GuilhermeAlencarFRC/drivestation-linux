#pragma once

#include "demo_session.h"
#include "preferences.h"

#include <QElapsedTimer>
#include <QSet>
#include <QWidget>
#include <array>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class QToolButton;

namespace ds {
class Battery;
class Lamp;
class TelemetryChart;

class MainWindow : public QWidget {
  Q_OBJECT
public:
  explicit MainWindow(PreferenceStore store = PreferenceStore(), QWidget* parent = nullptr);
  ~MainWindow() override;
  DemoSession& session() { return session_; }
  void selectTab(int tab);
  void selectRightView(int view);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  QWidget* createOperation();
  QWidget* createDiagnostics();
  QWidget* createSetup();
  QWidget* createUsb();
  QWidget* createPower();
  QWidget* createStatus();
  QWidget* createMessages();
  QWidget* createCharts();
  QWidget* createFooter();
  void refresh();
  void savePreferences();
  void appendMessage(const QString& message);
  void tick();

  PreferenceStore store_;
  Preferences preferences_;
  DemoSession session_;
  QElapsedTimer tickClock_;
  double lastChartSample_ = -1;
  bool settingsError_ = false;
  QSet<int> pressedKeys_;
  QStackedWidget* pages_ = nullptr;
  std::array<QToolButton*, 5> tabButtons_{};
  std::array<QToolButton*, 3> viewButtons_{};
  std::array<QRadioButton*, 4> modeButtons_{};
  QPushButton* enable_ = nullptr;
  QPushButton* disable_ = nullptr;
  QPushButton* stop_ = nullptr;
  QPushButton* reset_ = nullptr;
  QLabel* elapsed_ = nullptr;
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
  QListWidget* devices_ = nullptr;
  std::array<QProgressBar*, 6> axes_{};
  std::array<QLabel*, 8> buttons_{};
  QLabel* usbSource_ = nullptr;
  QLabel* diagnostics_ = nullptr;
  QWidget* messagePanel_ = nullptr;
  QWidget* chartPanel_ = nullptr;
  QPlainTextEdit* messages_ = nullptr;
  TelemetryChart* chart_ = nullptr;
};

void applyStationStyle(QWidget& widget);

} // namespace ds
