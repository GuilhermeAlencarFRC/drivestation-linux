#include "main_window.h"
#include "widgets.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>
#include <utility>

namespace ds {
namespace {
QLabel* label(const QString& text, QWidget* parent = nullptr) {
  auto* result = new QLabel(text, parent);
  result->setTextFormat(Qt::PlainText);
  return result;
}

QWidget* panel() {
  auto* result = new QFrame;
  result->setObjectName(QStringLiteral("panel"));
  return result;
}

QToolButton* navigation(Symbol symbol, const QString& name, const QString& objectName) {
  auto* result = new QToolButton;
  result->setObjectName(objectName);
  result->setIcon(symbolIcon(symbol));
  result->setIconSize({24, 24});
  result->setCheckable(true);
  result->setFixedWidth(34);
  result->setMinimumHeight(32);
  result->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  result->setProperty("navigation", true);
  result->setToolTip(name);
  result->setAccessibleName(name);
  return result;
}

QSpinBox* spinBox(int value, int maximum, const QString& name) {
  auto* result = new QSpinBox;
  result->setRange(0, maximum);
  result->setValue(value);
  result->setObjectName(name);
  result->setAccessibleName(name);
  result->setButtonSymbols(QAbstractSpinBox::NoButtons);
  result->setAlignment(Qt::AlignRight);
  return result;
}

QString formatTime(qint64 milliseconds) {
  return QStringLiteral("%1:%2.%3")
      .arg(milliseconds / 60000)
      .arg((milliseconds / 1000) % 60, 2, 10, QLatin1Char('0'))
      .arg((milliseconds / 100) % 10);
}
} // namespace

MainWindow::MainWindow(PreferenceStore store, QWidget* parent)
    : QWidget(parent), store_(std::move(store)), preferences_(store_.load()), session_(this) {
  setObjectName(QStringLiteral("stationWindow"));
  setWindowTitle(
      QStringLiteral("DriveStation Linux %1 — Offline preview").arg(DRIVESTATION_VERSION));
  setWindowIcon(symbolIcon(Symbol::Operation));
  setMinimumWidth(1024);
  setFixedHeight(224);
  resize(1100, 224);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(4, 4, 4, 3);
  root->setSpacing(2);
  auto* body = new QHBoxLayout;
  body->setSpacing(6);
  auto* navigationLayout = new QVBoxLayout;
  navigationLayout->setSpacing(1);
  auto* group = new QButtonGroup(this);
  const std::array<Symbol, 5> symbols{Symbol::Operation, Symbol::Diagnostics, Symbol::Setup,
                                      Symbol::Usb, Symbol::Power};
  const QStringList names{QStringLiteral("Operation"), QStringLiteral("Diagnostics"),
                          QStringLiteral("Setup"), QStringLiteral("USB Devices"),
                          QStringLiteral("CAN / Power")};
  for (int i = 0; i < 5; ++i) {
    tabButtons_[i] = navigation(symbols[i], names[i], QStringLiteral("tab%1").arg(i));
    group->addButton(tabButtons_[i], i);
    navigationLayout->addWidget(tabButtons_[i]);
  }
  connect(group, &QButtonGroup::idClicked, this, &MainWindow::selectTab);
  body->addLayout(navigationLayout);

  pages_ = new QStackedWidget;
  pages_->setObjectName(QStringLiteral("pages"));
  pages_->setFixedWidth(392);
  pages_->addWidget(createOperation());
  pages_->addWidget(createDiagnostics());
  pages_->addWidget(createSetup());
  pages_->addWidget(createUsb());
  pages_->addWidget(createPower());
  body->addWidget(pages_);
  body->addWidget(createStatus());

  auto* output = new QHBoxLayout;
  output->setSpacing(4);
  messagePanel_ = createMessages();
  chartPanel_ = createCharts();
  output->addWidget(messagePanel_, 1);
  output->addWidget(chartPanel_, 1);
  body->addLayout(output, 1);
  auto* viewLayout = new QVBoxLayout;
  viewLayout->setSpacing(1);
  auto* views = new QButtonGroup(this);
  const std::array<Symbol, 3> viewSymbols{Symbol::Messages, Symbol::Charts, Symbol::Both};
  const QStringList viewNames{QStringLiteral("Messages"), QStringLiteral("Charts"),
                              QStringLiteral("Both")};
  for (int i = 0; i < 3; ++i) {
    viewButtons_[i] = navigation(viewSymbols[i], viewNames[i], QStringLiteral("view%1").arg(i));
    views->addButton(viewButtons_[i], i);
    viewLayout->addWidget(viewButtons_[i]);
  }
  connect(views, &QButtonGroup::idClicked, this, &MainWindow::selectRightView);
  body->addLayout(viewLayout);
  root->addLayout(body, 1);
  root->addWidget(createFooter());

  applyStationStyle(*this);
  selectTab(0);
  selectRightView(0);
  session_.setPracticeTimings(preferences_.practiceSeconds);
  connect(&session_, &DemoSession::changed, this, &MainWindow::refresh);
  connect(qApp, &QGuiApplication::applicationStateChanged, this,
          [this](Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive) {
              pressedKeys_.clear();
              session_.disable();
            }
          });
  qApp->installEventFilter(this);
  auto* timer = new QTimer(this);
  timer->setObjectName(QStringLiteral("demoTicker"));
  timer->setInterval(100);
  connect(timer, &QTimer::timeout, this, &MainWindow::tick);
  tickClock_.start();
  timer->start();
  appendMessage(QStringLiteral("Offline preview. No robot transport is implemented."));
  appendMessage(QStringLiteral("Select Simulate robot to explore local demonstration data."));
  refresh();
}

MainWindow::~MainWindow() { qApp->removeEventFilter(this); }

void MainWindow::closeEvent(QCloseEvent* event) {
  pressedKeys_.clear();
  session_.disable();
  QWidget::closeEvent(event);
}

QWidget* MainWindow::createOperation() {
  auto* result = panel();
  auto* layout = new QHBoxLayout(result);
  layout->setContentsMargins(8, 8, 8, 6);
  layout->setSpacing(12);
  auto* left = new QVBoxLayout;
  left->setSpacing(2);
  auto* group = new QButtonGroup(this);
  for (int i = 0; i < 4; ++i) {
    auto mode = static_cast<DemoSession::Mode>(i);
    modeButtons_[i] = new QRadioButton(DemoSession::modeName(mode));
    modeButtons_[i]->setObjectName(QStringLiteral("mode%1").arg(i));
    group->addButton(modeButtons_[i], i);
    left->addWidget(modeButtons_[i]);
  }
  connect(group, &QButtonGroup::idClicked, this, [this](int id) {
    session_.setMode(static_cast<DemoSession::Mode>(id));
    appendMessage(
        QStringLiteral("Demo mode: %1 (disabled).").arg(DemoSession::modeName(session_.mode())));
  });
  left->addStretch();
  auto* commands = new QHBoxLayout;
  commands->setSpacing(2);
  enable_ = new QPushButton(QStringLiteral("Enable"));
  enable_->setObjectName(QStringLiteral("enable"));
  disable_ = new QPushButton(QStringLiteral("Disable"));
  disable_->setObjectName(QStringLiteral("disable"));
  commands->addWidget(enable_);
  commands->addWidget(disable_);
  left->addLayout(commands);
  auto* stops = new QHBoxLayout;
  stops->setSpacing(2);
  autoStop_ = new QPushButton(QStringLiteral("A-Stop"));
  autoStop_->setObjectName(QStringLiteral("autonomousStop"));
  autoStop_->setToolTip(QStringLiteral("Backspace: stop Practice Autonomous; resume at Teleop."));
  stops->addWidget(autoStop_);
  stop_ = new QPushButton(QStringLiteral("E-Stop"));
  stop_->setObjectName(QStringLiteral("emergencyStop"));
  stop_->setToolTip(QStringLiteral("Space: latch emergency stop until Reset demo."));
  stops->addWidget(stop_);
  left->addLayout(stops);
  connect(enable_, &QPushButton::clicked, this, &MainWindow::requestEnable);
  connect(disable_, &QPushButton::clicked, this, [this] { session_.disable(); });
  connect(stop_, &QPushButton::clicked, this, [this] { session_.emergencyStop(); });
  connect(autoStop_, &QPushButton::clicked, this, [this] { session_.autonomousStop(); });
  layout->addLayout(left, 1);
  auto* line = new QFrame;
  line->setFrameShape(QFrame::VLine);
  layout->addWidget(line);
  auto* right = new QGridLayout;
  right->setVerticalSpacing(5);
  elapsedCaption_ = label(QStringLiteral("Elapsed Time"));
  right->addWidget(elapsedCaption_, 0, 0);
  elapsed_ = label(QStringLiteral("0:00.0"));
  elapsed_->setObjectName(QStringLiteral("elapsed"));
  elapsed_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  right->addWidget(elapsed_, 0, 1);
  right->addWidget(label(QStringLiteral("Phase Left")), 1, 0);
  phaseRemaining_ = label(QStringLiteral("—"));
  phaseRemaining_->setObjectName(QStringLiteral("phaseRemaining"));
  right->addWidget(phaseRemaining_, 1, 1);
  right->addWidget(label(QStringLiteral("PC Battery")), 2, 0);
  right->addWidget(label(QStringLiteral("—")), 2, 1);
  right->addWidget(label(QStringLiteral("PC CPU %")), 3, 0);
  right->addWidget(label(QStringLiteral("—")), 3, 1);
  right->addWidget(label(QStringLiteral("Window")), 4, 0);
  right->addWidget(label(QStringLiteral("Floating")), 4, 1);
  right->addWidget(label(QStringLiteral("Team Station")), 5, 0);
  station_ = new QComboBox;
  station_->setObjectName(QStringLiteral("station"));
  station_->addItems({QStringLiteral("Red 1"), QStringLiteral("Red 2"), QStringLiteral("Red 3"),
                      QStringLiteral("Blue 1"), QStringLiteral("Blue 2"),
                      QStringLiteral("Blue 3")});
  station_->setCurrentIndex(preferences_.station);
  connect(station_, &QComboBox::currentIndexChanged, this, [this](int index) {
    preferences_.station = index;
    session_.disable();
    savePreferences();
  });
  right->addWidget(station_, 5, 1);
  layout->addLayout(right, 2);
  return result;
}

QWidget* MainWindow::createDiagnostics() {
  auto* result = panel();
  auto* layout = new QVBoxLayout(result);
  layout->setContentsMargins(10, 8, 10, 8);
  diagnostics_ = label({});
  diagnostics_->setObjectName(QStringLiteral("diagnostics"));
  diagnostics_->setWordWrap(true);
  layout->addWidget(diagnostics_, 1);
  auto* note = label(QStringLiteral("Network interfaces and robot services are not queried."));
  note->setWordWrap(true);
  note->setProperty("muted", true);
  layout->addWidget(note);
  auto* actions = new QHBoxLayout;
  for (const auto& text :
       {QStringLiteral("Reboot roboRIO"), QStringLiteral("Restart Robot Code")}) {
    auto* button = new QPushButton(text);
    button->setEnabled(false);
    button->setToolTip(
        QStringLiteral("Requires robot communication, not implemented in this preview."));
    actions->addWidget(button);
  }
  layout->addLayout(actions);
  return result;
}

QWidget* MainWindow::createSetup() {
  auto* result = panel();
  auto* layout = new QHBoxLayout(result);
  layout->setContentsMargins(8, 6, 8, 6);
  layout->setSpacing(16);
  auto* fields = new QVBoxLayout;
  fields->setSpacing(2);
  fields->addWidget(label(QStringLiteral("Team Number")));
  teamInput_ = spinBox(preferences_.team, 65535, QStringLiteral("teamNumber"));
  fields->addWidget(teamInput_);
  fields->addWidget(label(QStringLiteral("Dashboard Type")));
  dashboard_ = new QComboBox;
  dashboard_->setObjectName(QStringLiteral("dashboard"));
  dashboard_->addItems({QStringLiteral("None"), QStringLiteral("SmartDashboard"),
                        QStringLiteral("Shuffleboard"), QStringLiteral("Elastic"),
                        QStringLiteral("Custom")});
  dashboard_->setCurrentIndex(preferences_.dashboard);
  dashboard_->setToolTip(QStringLiteral("Saved preference; dashboard launch is not implemented."));
  fields->addWidget(dashboard_);
  fields->addStretch();
  fields->addWidget(label(QStringLiteral("Game Data")));
  gameData_ = new QLineEdit(preferences_.gameData);
  gameData_->setObjectName(QStringLiteral("gameData"));
  gameData_->setMaxLength(256);
  fields->addWidget(gameData_);
  layout->addLayout(fields, 1);

  auto* timing = new QVBoxLayout;
  timing->setSpacing(1);
  timing->addWidget(label(QStringLiteral("Practice Timing (s)")));
  auto* grid = new QGridLayout;
  grid->setVerticalSpacing(2);
  const QStringList stages{QStringLiteral("Countdown"), QStringLiteral("Autonomous"),
                           QStringLiteral("Delay"), QStringLiteral("Teleoperated"),
                           QStringLiteral("End Game")};
  for (int i = 0; i < 5; ++i) {
    grid->addWidget(label(stages[i]), i, 0);
    auto* input =
        spinBox(preferences_.practiceSeconds[i], 600, QStringLiteral("practice%1").arg(i));
    input->setFixedWidth(42);
    grid->addWidget(input, i, 1);
    connect(input, &QSpinBox::valueChanged, this, [this, i](int value) {
      preferences_.practiceSeconds[i] = value;
      session_.setPracticeTimings(preferences_.practiceSeconds);
      savePreferences();
    });
  }
  timing->addLayout(grid);
  auto* note = label(QStringLiteral("Local sequence only"));
  note->setProperty("muted", true);
  timing->addWidget(note);
  timing->addStretch();
  layout->addLayout(timing, 1);
  connect(teamInput_, &QSpinBox::valueChanged, this, [this](int team) {
    preferences_.team = team;
    session_.disable();
    savePreferences();
    refresh();
  });
  connect(dashboard_, &QComboBox::currentIndexChanged, this, [this](int index) {
    preferences_.dashboard = index;
    savePreferences();
  });
  connect(gameData_, &QLineEdit::textChanged, this, [this](const QString& data) {
    preferences_.gameData = data;
    savePreferences();
  });
  return result;
}

QWidget* MainWindow::createUsb() {
  auto* result = panel();
  auto* layout = new QHBoxLayout(result);
  layout->setContentsMargins(8, 6, 8, 6);
  layout->setSpacing(10);
  auto* left = new QVBoxLayout;
  left->setSpacing(3);
  left->addWidget(label(QStringLiteral("USB Order")));
  devices_ = new QListWidget;
  devices_->setObjectName(QStringLiteral("devices"));
  devices_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  for (int i = 0; i < 6; ++i)
    devices_->addItem(QStringLiteral("%1  —").arg(i));
  left->addWidget(devices_, 1);
  auto* rescan = new QPushButton(QStringLiteral("Rescan (F1)"));
  rescan->setEnabled(false);
  rescan->setToolTip(QStringLiteral("Physical joystick discovery is not implemented."));
  left->addWidget(rescan);
  layout->addLayout(left, 1);
  auto* right = new QVBoxLayout;
  right->setSpacing(3);
  right->addWidget(label(QStringLiteral("Axes              Buttons")));
  auto* axesGrid = new QGridLayout;
  axesGrid->setVerticalSpacing(4);
  for (int i = 0; i < 6; ++i) {
    axes_[i] = new QProgressBar;
    axes_[i]->setRange(0, 200);
    axes_[i]->setFixedHeight(14);
    axes_[i]->setAccessibleName(QStringLiteral("Simulated axis %1").arg(i));
    axesGrid->addWidget(axes_[i], i, 0);
  }
  for (int i = 0; i < 8; ++i) {
    buttons_[i] = label(QString::number(i + 1));
    buttons_[i]->setAlignment(Qt::AlignCenter);
    buttons_[i]->setFixedSize(16, 14);
    axesGrid->addWidget(buttons_[i], i % 4, 1 + i / 4);
  }
  right->addLayout(axesGrid);
  usbSource_ = label(QStringLiteral("Hardware not scanned"));
  usbSource_->setWordWrap(true);
  usbSource_->setProperty("muted", true);
  right->addWidget(usbSource_);
  right->addStretch();
  layout->addLayout(right, 1);
  return result;
}

QWidget* MainWindow::createPower() {
  auto* result = panel();
  auto* layout = new QVBoxLayout(result);
  layout->setContentsMargins(12, 9, 12, 8);
  auto* grid = new QGridLayout;
  grid->setVerticalSpacing(10);
  const QStringList left{QStringLiteral("Disable #"), QStringLiteral("Comms    —"),
                         QStringLiteral("12V         —"), QStringLiteral("6V / 5V   —"),
                         QStringLiteral("3.3V        —")};
  const QStringList right{QStringLiteral("CAN Metrics"), QStringLiteral("Utilization %    —"),
                          QStringLiteral("Bus Off             —"),
                          QStringLiteral("TX Full              —"),
                          QStringLiteral("Receive / TX     —")};
  for (int i = 0; i < 5; ++i) {
    grid->addWidget(label(left[i]), i, 0);
    grid->addWidget(label(right[i]), i, 1);
  }
  layout->addLayout(grid);
  layout->addStretch();
  auto* note = label(QStringLiteral("No CAN or power telemetry received."));
  note->setProperty("muted", true);
  layout->addWidget(note);
  return result;
}

QWidget* MainWindow::createStatus() {
  auto* result = new QWidget;
  result->setFixedWidth(158);
  auto* layout = new QVBoxLayout(result);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);
  auto* teamRow = new QHBoxLayout;
  teamRow->addWidget(label(QStringLiteral("Team #")));
  team_ = label({});
  team_->setObjectName(QStringLiteral("teamStatus"));
  team_->setAlignment(Qt::AlignRight);
  teamRow->addWidget(team_);
  layout->addLayout(teamRow);
  auto* batteryRow = new QHBoxLayout;
  battery_ = new Battery;
  battery_->setFixedSize(63, 36);
  batteryRow->addWidget(battery_);
  voltage_ = label(QStringLiteral("--.--"));
  voltage_->setObjectName(QStringLiteral("voltage"));
  voltage_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  batteryRow->addWidget(voltage_, 1);
  layout->addLayout(batteryRow);
  const QStringList names{QStringLiteral("Communications"), QStringLiteral("Robot Code"),
                          QStringLiteral("Joysticks")};
  for (int i = 0; i < 3; ++i) {
    lamps_[i] = new Lamp(names[i]);
    layout->addWidget(lamps_[i]);
  }
  layout->addStretch();
  status_ = label({});
  status_->setObjectName(QStringLiteral("robotStatus"));
  status_->setWordWrap(true);
  status_->setAlignment(Qt::AlignCenter);
  status_->setMinimumHeight(34);
  layout->addWidget(status_);
  dataSource_ = label(QStringLiteral("OFFLINE"));
  dataSource_->setObjectName(QStringLiteral("dataSource"));
  dataSource_->setAlignment(Qt::AlignCenter);
  layout->addWidget(dataSource_);
  return result;
}

QWidget* MainWindow::createMessages() {
  auto* result = panel();
  result->setObjectName(QStringLiteral("messagePanel"));
  auto* layout = new QVBoxLayout(result);
  layout->setContentsMargins(7, 4, 7, 6);
  layout->setSpacing(3);
  auto* header = new QHBoxLayout;
  header->addWidget(label(QStringLiteral("Messages")), 1);
  auto* clear = new QPushButton(QStringLiteral("Clear"));
  clear->setObjectName(QStringLiteral("clearMessages"));
  clear->setFixedHeight(22);
  header->addWidget(clear);
  layout->addLayout(header);
  messages_ = new QPlainTextEdit;
  messages_->setObjectName(QStringLiteral("messages"));
  messages_->setReadOnly(true);
  messages_->setMaximumBlockCount(200);
  layout->addWidget(messages_, 1);
  connect(clear, &QPushButton::clicked, messages_, &QPlainTextEdit::clear);
  return result;
}

QWidget* MainWindow::createCharts() {
  auto* result = panel();
  result->setObjectName(QStringLiteral("chartPanel"));
  auto* layout = new QVBoxLayout(result);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(0);
  auto* toolbar = new QHBoxLayout;
  toolbar->setSpacing(2);
  auto* group = new QButtonGroup(this);
  const std::array<int, 3> windows{12, 60, 300};
  const QStringList captions{QStringLiteral("12 s"), QStringLiteral("1 m"), QStringLiteral("5 m")};
  for (int i = 0; i < 3; ++i) {
    auto* button = new QPushButton(captions[i]);
    button->setCheckable(true);
    button->setFixedHeight(21);
    group->addButton(button, windows[i]);
    toolbar->addWidget(button);
    if (i == 0)
      button->setChecked(true);
  }
  layout->addLayout(toolbar);
  chart_ = new TelemetryChart;
  layout->addWidget(chart_, 1);
  connect(group, &QButtonGroup::idClicked, this,
          [this](int seconds) { chart_->setWindowSeconds(seconds); });
  return result;
}

QWidget* MainWindow::createFooter() {
  auto* result = new QWidget;
  auto* layout = new QHBoxLayout(result);
  layout->setContentsMargins(2, 0, 2, 0);
  layout->setSpacing(8);
  footer_ = label(QStringLiteral("OFFLINE PREVIEW · No robot commands"));
  footer_->setObjectName(QStringLiteral("footer"));
  layout->addWidget(footer_, 1);
  simulate_ = new QCheckBox(QStringLiteral("Simulate robot"));
  simulate_->setObjectName(QStringLiteral("simulateRobot"));
  simulate_->setToolTip(
      QStringLiteral("Local generated data only. No hardware or network is used."));
  layout->addWidget(simulate_);
  reset_ = new QPushButton(QStringLiteral("Reset demo"));
  reset_->setObjectName(QStringLiteral("resetDemo"));
  reset_->setFixedHeight(21);
  layout->addWidget(reset_);
  connect(simulate_, &QCheckBox::toggled, this, [this](bool enabled) {
    session_.setConnected(enabled);
    tickClock_.restart();
    appendMessage(enabled ? QStringLiteral("Simulated robot connected (local data).")
                          : QStringLiteral("Simulated robot disconnected; demo disabled."));
  });
  connect(reset_, &QPushButton::clicked, this, [this] {
    session_.reset();
    chart_->clear();
    lastChartSample_ = -1;
    pressedKeys_.clear();
    appendMessage(QStringLiteral("Demo reset. Disconnected and disabled."));
  });
  return result;
}

void MainWindow::selectTab(int tab) {
  if (tab < 0 || tab >= static_cast<int>(tabButtons_.size()))
    return;
  pages_->setCurrentIndex(tab);
  tabButtons_[tab]->setChecked(true);
}

void MainWindow::selectRightView(int view) {
  if (view < 0 || view >= static_cast<int>(viewButtons_.size()))
    return;
  viewButtons_[view]->setChecked(true);
  messagePanel_->setVisible(view != 1);
  chartPanel_->setVisible(view != 0);
}

void MainWindow::savePreferences() {
  settingsError_ = !store_.save(preferences_);
  if (settingsError_)
    appendMessage(
        QStringLiteral("Could not save preferences. Check configuration file permissions."));
  refresh();
}

void MainWindow::appendMessage(const QString& message) {
  messages_->appendPlainText(QStringLiteral("[%1] %2").arg(
      QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), message));
}

void MainWindow::requestEnable() {
  if (session_.enable()) {
    tickClock_.restart();
    appendMessage(session_.mode() == DemoSession::Mode::Practice
                      ? QStringLiteral("Practice started locally. No commands sent.")
                      : QStringLiteral("Demo enabled. No commands sent."));
  }
}

void MainWindow::refresh() {
  const bool connected = session_.connected();
  const QSignalBlocker blocker(simulate_);
  simulate_->setChecked(connected);
  team_->setText(QString::number(preferences_.team));
  voltage_->setText(connected ? QStringLiteral("%1 V").arg(session_.voltage(), 0, 'f', 2)
                              : QStringLiteral("--.--"));
  battery_->setVoltage(session_.voltage(), connected);
  for (auto* lamp : lamps_)
    lamp->setLit(connected);
  status_->setText(session_.status());
  const bool practice = session_.mode() == DemoSession::Mode::Practice;
  const bool pending = practice && connected && session_.practice().pendingEnable();
  dataSource_->setText(!connected           ? QStringLiteral("OFFLINE")
                       : pending            ? QStringLiteral("SIMULATED · WILL ENABLE")
                       : session_.enabled() ? QStringLiteral("SIMULATED · ENABLED")
                                            : QStringLiteral("SIMULATED · DISABLED"));
  const bool flash = pending && (static_cast<qint64>(session_.simulationSeconds() * 2) % 2 == 0);
  status_->setStyleSheet(session_.emergencyStopped()
                             ? QStringLiteral("color: #ff625a; font-weight: bold;")
                         : flash ? QStringLiteral("background: #a35900; font-weight: bold;")
                                 : QStringLiteral("font-weight: bold;"));
  modeButtons_[static_cast<int>(session_.mode())]->setChecked(true);
  enable_->setEnabled(session_.canEnable());
  enable_->setToolTip(
      session_.mode() == DemoSession::Mode::Practice
          ? QStringLiteral("Start the local Practice sequence using the times in Setup.")
          : QStringLiteral("Enable the local demonstration. Shortcut: hold [ ] \\."));
  disable_->setEnabled(session_.busy());
  stop_->setEnabled(connected && !session_.emergencyStopped());
  autoStop_->setEnabled(practice && session_.enabled() &&
                        session_.practice().phase() == PracticeSequence::Phase::Autonomous);
  reset_->setEnabled(connected || session_.emergencyStopped() || session_.elapsedMs() != 0);
  elapsedCaption_->setText(practice ? QStringLiteral("Sequence Time")
                                    : QStringLiteral("Elapsed Time"));
  elapsed_->setText(formatTime(session_.elapsedMs()));
  phaseRemaining_->setText(practice && session_.practice().active()
                               ? formatTime(session_.practice().remainingMs())
                               : QStringLiteral("—"));
  footer_->setText(settingsError_ ? QStringLiteral("PREFERENCES NOT SAVED · Check file permissions")
                                  : QStringLiteral("OFFLINE PREVIEW · No robot commands · Enter: "
                                                   "Disable / Space: E-Stop / Backspace: A-Stop"));
  if (session_.practice().phase() != lastPracticePhase_) {
    lastPracticePhase_ = session_.practice().phase();
    appendMessage(
        QStringLiteral("Practice: %1 (local).").arg(DemoSession::phaseName(lastPracticePhase_)));
  }
  if (session_.practice().autoStopped() && !lastAutoStopped_) {
    appendMessage(QStringLiteral("A-Stop: Autonomous disabled; Teleop will resume automatically."));
  }
  lastAutoStopped_ = session_.practice().autoStopped();
  diagnostics_->setText(QStringLiteral("Communications                     Versions & Info\n\n"
                                       "Robot: %1\nFMS: Not connected\nDS: %2 (offline preview)\n"
                                       "roboRIO image / WPILib: Not received")
                            .arg(connected ? QStringLiteral("Simulated (no network)")
                                           : QStringLiteral("Not connected"),
                                 DRIVESTATION_VERSION));
  devices_->item(0)->setText(connected ? QStringLiteral("0  Demo controller")
                                       : QStringLiteral("0  —"));
  devices_->item(0)->setForeground(connected ? QColor("#72e22d") : QColor("#bcbcbc"));
  usbSource_->setText(connected ? QStringLiteral("SIMULATED inputs\nNo physical devices read")
                                : QStringLiteral("Hardware not scanned"));
  for (int i = 0; i < 6; ++i) {
    const double value = connected ? std::sin(session_.simulationSeconds() + i * 0.7) * 0.6 : 0;
    axes_[i]->setValue(static_cast<int>((value + 1) * 100));
    axes_[i]->setFormat(connected ? QString::number(value, 'f', 2) : QStringLiteral("—"));
  }
  for (int i = 0; i < 8; ++i) {
    const bool pressed = connected && (static_cast<int>(session_.simulationSeconds()) % 8 == i);
    buttons_[i]->setStyleSheet(pressed ? QStringLiteral("background: #56bb1d; color: #111;")
                                       : QStringLiteral("background: #303030; color: #888;"));
  }
}

void MainWindow::tick() {
  const qint64 elapsed = tickClock_.restart();
  const bool cancelled = elapsed > DemoSession::MaxTickGapMs && session_.busy();
  session_.advance(elapsed);
  if (cancelled)
    appendMessage(
        QStringLiteral("Demo cancelled after delayed update; enable explicitly to restart."));
  if (session_.connected() && session_.simulationSeconds() - lastChartSample_ >= 0.2) {
    chart_->appendSample(session_.simulationSeconds(), session_.tripTime(), session_.voltage());
    lastChartSample_ = session_.simulationSeconds();
  }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::ApplicationDeactivate || event->type() == QEvent::WindowDeactivate) {
    pressedKeys_.clear();
    session_.disable();
  }
  if (event->type() == QEvent::FocusIn)
    pressedKeys_.clear();
  auto* widget = qobject_cast<QWidget*>(watched);
  if (!widget || (widget != this && !isAncestorOf(widget)))
    return QWidget::eventFilter(watched, event);
  if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease)
    return QWidget::eventFilter(watched, event);
  auto* key = static_cast<QKeyEvent*>(event);
  if (key->isAutoRepeat())
    return QWidget::eventFilter(watched, event);
  if (event->type() == QEvent::KeyRelease) {
    pressedKeys_.remove(key->key());
    return QWidget::eventFilter(watched, event);
  }
  const bool editing = qobject_cast<QLineEdit*>(widget) || qobject_cast<QSpinBox*>(widget);
  if (key->key() == Qt::Key_Backspace && !editing && session_.autonomousStop()) {
    pressedKeys_.clear();
    return true;
  }
  if (key->key() == Qt::Key_Space && session_.connected()) {
    pressedKeys_.clear();
    session_.emergencyStop();
    return true;
  }
  if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
    pressedKeys_.clear();
    session_.disable();
    // Preserve editingFinished on team number and other text fields.
    if (!qobject_cast<QLineEdit*>(widget) && !qobject_cast<QSpinBox*>(widget))
      return true;
  }
  if (!editing && (key->key() == Qt::Key_BracketLeft || key->key() == Qt::Key_BracketRight ||
                   key->key() == Qt::Key_Backslash)) {
    pressedKeys_.insert(key->key());
    if (pressedKeys_.contains(Qt::Key_BracketLeft) && pressedKeys_.contains(Qt::Key_BracketRight) &&
        pressedKeys_.contains(Qt::Key_Backslash)) {
      requestEnable();
      pressedKeys_.clear();
    }
    return true;
  }
  return QWidget::eventFilter(watched, event);
}

void applyStationStyle(QWidget& widget) {
  widget.setStyleSheet(QStringLiteral(R"(
    QWidget { color: #ededed; font-family: "DejaVu Sans", "Sans Serif"; font-size: 11px; }
    QWidget#stationWindow { background: #303030; }
    QFrame#panel, QWidget#messagePanel, QWidget#chartPanel {
      background: #444444; border: 1px solid #242424; border-radius: 4px;
    }
    QLabel, QCheckBox, QRadioButton { background: transparent; border: none; }
    QLabel[muted="true"] { color: #b8b8b8; font-size: 10px; }
    QLabel#robotStatus { font-size: 13px; }
    QLabel#dataSource { color: #00c9e8; font-size: 9px; font-weight: bold; }
    QLabel#footer { color: #bbb; font-size: 9px; }
    QLabel#elapsed, QLabel#voltage, QLabel#teamStatus { font-size: 17px; }
    QToolButton[navigation="true"] {
      background: #383838; border: 1px solid #292929; border-radius: 3px; padding: 1px;
    }
    QToolButton[navigation="true"]:checked { border-right: 3px solid #00c9e8; background: #474747; }
    QToolButton:hover, QPushButton:hover { background: #555; }
    QToolButton:focus, QPushButton:focus { border: 1px solid #00c9e8; }
    QPushButton { background: #393939; border: 1px solid #282828; padding: 3px 5px; }
    QPushButton:pressed, QPushButton:checked { background: #252525; border: 1px solid #00c9e8; }
    QPushButton:disabled { color: #858585; }
    QPushButton#enable { color: #61e033; font-weight: bold; font-size: 14px; min-height: 23px; }
    QPushButton#enable:disabled { color: #507b40; }
    QPushButton#disable { color: #ff3a32; font-weight: bold; font-size: 14px; min-height: 23px; }
    QPushButton#disable:disabled { color: #a25550; }
    QPushButton#emergencyStop { color: #ff625a; }
    QRadioButton { spacing: 5px; padding: 2px 0; }
    QRadioButton::indicator { width: 10px; height: 10px; border: 1px solid #888; border-radius: 5px; background: #303030; }
    QRadioButton::indicator:checked { background: #00c9e8; border-color: #00c9e8; }
    QLineEdit, QComboBox { color: #111; background: #ccc; border: 1px solid #333; padding: 2px 4px; min-height: 16px; }
    QSpinBox { color: #111; background: #ccc; min-height: 20px; }
    QSpinBox QLineEdit { border: none; padding: 0; min-height: 0; background: transparent; }
    QComboBox QAbstractItemView { color: #eee; background: #444; selection-background-color: #176878; }
    QListWidget { background: #484848; border: 1px solid #666; font-size: 10px; }
    QListWidget::item { padding: 1px 2px; }
    QPlainTextEdit { background: #2c2c2c; color: #ddd; border: none; font-family: "DejaVu Sans Mono", monospace; font-size: 10px; }
    QProgressBar { background: #303030; border: 1px solid #222; text-align: center; font-size: 9px; }
    QProgressBar::chunk { background: #26761b; }
    QCheckBox { spacing: 4px; font-size: 10px; }
    QCheckBox::indicator { width: 11px; height: 11px; background: #555; border: 1px solid #999; }
    QCheckBox::indicator:checked { background: #00a9c5; border: 1px solid #00d5f5; }
    QToolTip { color: #eee; background: #242424; border: 1px solid #666; }
  )"));
}

} // namespace ds
