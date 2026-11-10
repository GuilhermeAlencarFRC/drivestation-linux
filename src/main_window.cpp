#include "main_window.h"
#include "widgets.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>
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
  QIcon icon;
  icon.addPixmap(symbolIcon(symbol, QColor("#999999")).pixmap(24, 24), QIcon::Normal, QIcon::Off);
  icon.addPixmap(symbolIcon(symbol, QColor("#00c9e8")).pixmap(24, 24), QIcon::Normal, QIcon::On);
  result->setIcon(icon);
  result->setIconSize({20, 20});
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

QString stationTitle(const QString& source) {
  return QStringLiteral("DriveStation Linux %1 — ALFA — VERSÃO DE TESTES — %2")
      .arg(DRIVESTATION_VERSION, source);
}
} // namespace

MainWindow::MainWindow(PreferenceStore store, QWidget* parent,
                       std::unique_ptr<ControllerBackend> backend, DiscoveryTarget discoveryTarget)
    : QWidget(parent), store_(std::move(store)), preferences_(store_.load()), session_(this),
      controllers_(std::move(backend), this), network_(controllers_, this),
      discovery_(this, std::move(discoveryTarget)) {
  setObjectName(QStringLiteral("stationWindow"));
  setWindowTitle(stationTitle(QStringLiteral("Offline preview")));
  setWindowIcon(symbolIcon(Symbol::Operation));
  setMinimumWidth(1024);
  setFixedHeight(200);
  resize(1024, 200);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(0, 4, 0, 4);
  root->setSpacing(0);
  auto* body = new QHBoxLayout;
  body->setSpacing(10);
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
    tabButtons_[i]->setFixedWidth(36);
    group->addButton(tabButtons_[i], i);
    navigationLayout->addWidget(tabButtons_[i]);
  }
  connect(group, &QButtonGroup::idClicked, this, &MainWindow::selectTab);
  body->addLayout(navigationLayout);

  pages_ = new QStackedWidget;
  pages_->setObjectName(QStringLiteral("pages"));
  pages_->setFixedWidth(398);
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
  auto* closeButton = new QToolButton;
  closeButton->setObjectName(QStringLiteral("closeStation"));
  closeButton->setText(QStringLiteral("×"));
  closeButton->setFixedSize(34, 28);
  closeButton->setToolTip(QStringLiteral("Close Driver Station"));
  connect(closeButton, &QToolButton::clicked, this, &QWidget::close);
  viewLayout->addWidget(closeButton);
  auto* views = new QButtonGroup(this);
  const std::array<Symbol, 3> viewSymbols{Symbol::Messages, Symbol::Charts, Symbol::Both};
  const QStringList viewNames{QStringLiteral("Messages"), QStringLiteral("Charts"),
                              QStringLiteral("Both")};
  for (int i = 0; i < 3; ++i) {
    viewButtons_[i] = navigation(viewSymbols[i], viewNames[i], QStringLiteral("view%1").arg(i));
    viewButtons_[i]->setProperty("rightSide", true);
    views->addButton(viewButtons_[i], i);
    viewLayout->addWidget(viewButtons_[i]);
  }
  connect(views, &QButtonGroup::idClicked, this, &MainWindow::selectRightView);
  body->addLayout(viewLayout);
  root->addLayout(body, 1);
  setContextMenuPolicy(Qt::CustomContextMenu);
  connect(this, &QWidget::customContextMenuRequested, this,
          [this](const QPoint& point) { controlsMenu_->popup(mapToGlobal(point)); });
  auto* menuShortcut = new QShortcut(QKeySequence(Qt::Key_F10), this);
  connect(menuShortcut, &QShortcut::activated, this,
          [this] { controlsMenu_->popup(mapToGlobal(QPoint(width() - 390, 24))); });

  applyStationStyle(*this);
  selectTab(0);
  selectRightView(0);
  session_.setPracticeTimings(preferences_.practiceSeconds);
  connect(&session_, &DemoSession::changed, this, &MainWindow::refresh);
  network_.setStation(static_cast<quint8>(preferences_.station));
  network_.setGameData(preferences_.gameData);
  connect(&network_, &NetworkSession::changed, this, &MainWindow::refresh);
  connect(&network_, &NetworkSession::telemetrySampled, this, [this](const TelemetrySample& sample) {
    if (usingNetwork_)
      chart_->appendSample(sample.elapsedMs / 1000.0, double(sample.roundTripMs),
                            sample.status.voltage(), sample.startsSegment);
  });
  connect(&discovery_, &RobotDiscovery::changed, this, &MainWindow::refreshDiscovery);
  connect(&network_, &NetworkSession::errorOccurred, this, [this](const QString& error) {
    appendMessage(QStringLiteral("Network: %1").arg(error));
  });
  connect(&network_, &NetworkSession::messageReceived, this, [this](const tcp::Message& message) {
    if (message.kind == tcp::Message::Kind::Version) {
      if (!message.endOfVersions)
        appendMessage(QStringLiteral("TCP version %1: %2").arg(message.name, message.version));
      return;
    }
    const auto kind = message.kind == tcp::Message::Kind::Stdout ? QStringLiteral("stdout")
                      : message.isError                          ? QStringLiteral("error")
                                                                 : QStringLiteral("warning");
    auto text = QStringLiteral("TCP %1 #%2 (%3 s): %4")
                    .arg(kind)
                    .arg(message.sequence)
                    .arg(message.seconds)
                    .arg(message.text);
    if (message.kind == tcp::Message::Kind::Error) {
      text += QStringLiteral("\nCode: %1").arg(message.errorCode);
      if (!message.location.isEmpty())
        text += QStringLiteral("\n") + message.location;
      if (!message.stack.isEmpty())
        text += QStringLiteral("\n") + message.stack;
    }
    appendMessage(text);
  });
  connect(qApp, &QGuiApplication::applicationStateChanged, this,
          [this](Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive) {
              stopRumble();
              pressedKeys_.clear();
              disableOperation();
            }
          });
  connect(&controllers_, &ControllerHub::topologyChanging, this, [this] {
    stopRumble();
    pressedKeys_.clear();
    disableOperation();
    appendMessage(QStringLiteral("USB devices changed or became unavailable. Operation disabled."));
  });
  connect(&controllers_, &ControllerHub::updated, this, &MainWindow::refreshUsb);
  connect(&controllers_, &ControllerHub::errorOccurred, this,
          [this](const QString& error) { appendMessage(QStringLiteral("USB: %1").arg(error)); });
  connect(controlsMenu_, &QMenu::aboutToShow, this, &MainWindow::stopRumble);
  controllers_.start();
  qApp->installEventFilter(this);
  auto* timer = new QTimer(this);
  timer->setObjectName(QStringLiteral("demoTicker"));
  timer->setInterval(100);
  connect(timer, &QTimer::timeout, this, &MainWindow::tick);
  tickClock_.start();
  timer->start();
  refresh();
}

MainWindow::~MainWindow() {
  stopRumble();
  discovery_.cancel();
  network_.stop();
  qApp->removeEventFilter(this);
}

void MainWindow::closeEvent(QCloseEvent* event) {
  stopRumble();
  pressedKeys_.clear();
  disableOperation();
  discovery_.cancel();
  network_.stop();
  QWidget::closeEvent(event);
}

QWidget* MainWindow::createOperation() {
  auto* result = panel();
  auto* layout = new QHBoxLayout(result);
  layout->setContentsMargins(10, 10, 10, 14);
  layout->setSpacing(18);
  auto* operationColumn = new QWidget;
  operationColumn->setFixedWidth(154);
  auto* columnLayout = new QVBoxLayout(operationColumn);
  columnLayout->setContentsMargins(0, 0, 0, 0);
  columnLayout->setSpacing(0);
  auto* left = new QVBoxLayout;
  left->setSpacing(0);
  auto* group = new QButtonGroup(this);
  for (int i = 0; i < 4; ++i) {
    auto mode = static_cast<DemoSession::Mode>(i);
    modeButtons_[i] =
        new QPushButton(mode == DemoSession::Mode::Teleoperated ? QStringLiteral("TeleOperated")
                                                                : DemoSession::modeName(mode));
    modeButtons_[i]->setObjectName(QStringLiteral("mode%1").arg(i));
    modeButtons_[i]->setProperty("modeSelector", true);
    modeButtons_[i]->setCheckable(true);
    modeButtons_[i]->setFixedHeight(23);
    group->addButton(modeButtons_[i], i);
    left->addWidget(modeButtons_[i]);
  }
  connect(group, &QButtonGroup::idClicked, this, [this](int id) {
    if (usingNetwork_) {
      if (id == static_cast<int>(DemoSession::Mode::Practice))
        return;
      network_.setMode(id == 1   ? wire::Mode::Autonomous
                       : id == 3 ? wire::Mode::Test
                                 : wire::Mode::Teleoperated);
      appendMessage(QStringLiteral("UDP mode changed; enable explicitly to operate."));
      return;
    }
    session_.setMode(static_cast<DemoSession::Mode>(id));
    appendMessage(
        QStringLiteral("Demo mode: %1 (disabled).").arg(DemoSession::modeName(session_.mode())));
  });
  columnLayout->addLayout(left);
  columnLayout->addStretch();
  auto* commandPanel = new QFrame;
  commandPanel->setObjectName(QStringLiteral("commandPanel"));
  commandPanel->setFixedHeight(56);
  auto* commands = new QHBoxLayout(commandPanel);
  commands->setContentsMargins(4, 0, 4, 0);
  commands->setSpacing(0);
  enable_ = new QPushButton(QStringLiteral("Enable"));
  enable_->setObjectName(QStringLiteral("enable"));
  enable_->setCheckable(true);
  enable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  disable_ = new QPushButton(QStringLiteral("Disable"));
  disable_->setObjectName(QStringLiteral("disable"));
  disable_->setCheckable(true);
  disable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  commands->addWidget(enable_);
  commands->addWidget(disable_);
  columnLayout->addWidget(commandPanel);
  autoStop_ = new QPushButton(QStringLiteral("A-Stop"));
  autoStop_->setObjectName(QStringLiteral("autonomousStop"));
  autoStop_->setToolTip(QStringLiteral("Backspace: stop Practice Autonomous; resume at Teleop."));
  stop_ = new QPushButton(QStringLiteral("E-Stop"));
  stop_->setObjectName(QStringLiteral("emergencyStop"));
  stop_->setToolTip(
      QStringLiteral("Space: latch emergency stop. UDP requires application restart."));
  connect(enable_, &QPushButton::clicked, this, &MainWindow::requestEnable);
  connect(disable_, &QPushButton::clicked, this, &MainWindow::disableOperation);
  connect(stop_, &QPushButton::clicked, this, &MainWindow::emergencyStopOperation);
  connect(autoStop_, &QPushButton::clicked, this, [this] { session_.autonomousStop(); });
  layout->addWidget(operationColumn);
  auto* line = new QFrame;
  line->setFrameShape(QFrame::VLine);
  layout->addWidget(line);
  auto* right = new QVBoxLayout;
  right->setSpacing(8);
  auto* elapsedRow = new QHBoxLayout;
  elapsedRow->setSpacing(3);
  elapsedCaption_ = label(QStringLiteral("Elapsed Time"));
  elapsedRow->addWidget(elapsedCaption_);
  elapsed_ = label(QStringLiteral("0:00.0"));
  elapsed_->setObjectName(QStringLiteral("elapsed"));
  elapsed_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  elapsedRow->addWidget(elapsed_);
  right->addLayout(elapsedRow);
  right->addSpacing(14);
  auto* hostMetrics = new QGridLayout;
  hostMetrics->setVerticalSpacing(4);
  const QStringList metricNames{QStringLiteral("PC Battery"), QStringLiteral("PC CPU %")};
  for (int i = 0; i < 2; ++i) {
    auto* name = label(metricNames[i]);
    name->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hostMetrics->addWidget(name, i, 0);
    auto* bar = new QProgressBar;
    bar->setProperty("hostMetric", true);
    bar->setFixedSize(88, 11);
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setTextVisible(false);
    bar->setToolTip(QStringLiteral("Host telemetry unavailable in this preview."));
    hostMetrics->addWidget(bar, i, 1);
  }
  right->addLayout(hostMetrics);
  right->addStretch();
  auto* windowRow = new QHBoxLayout;
  windowRow->addStretch();
  windowRow->addWidget(label(QStringLiteral("Window")));
  for (const auto& caption : {QStringLiteral("↤"), QStringLiteral("▭")}) {
    auto* button = new QToolButton;
    button->setProperty("windowControl", true);
    button->setText(caption);
    button->setFixedSize(40, 24);
    button->setEnabled(false);
    button->setToolTip(QStringLiteral("Floating window; docking is not implemented."));
    windowRow->addWidget(button);
  }
  right->addLayout(windowRow);
  right->addSpacing(6);
  auto* stationRow = new QHBoxLayout;
  stationRow->addWidget(label(QStringLiteral("Team Station")));
  station_ = new QComboBox;
  station_->setObjectName(QStringLiteral("station"));
  station_->addItems({QStringLiteral("Red 1"), QStringLiteral("Red 2"), QStringLiteral("Red 3"),
                      QStringLiteral("Blue 1"), QStringLiteral("Blue 2"),
                      QStringLiteral("Blue 3")});
  station_->setCurrentIndex(preferences_.station);
  connect(station_, &QComboBox::currentIndexChanged, this, [this](int index) {
    preferences_.station = index;
    disableOperation();
    network_.setStation(static_cast<quint8>(index));
    savePreferences();
  });
  station_->setFixedWidth(88);
  stationRow->addWidget(station_);
  right->addLayout(stationRow);
  layout->addLayout(right, 1);
  return result;
}

QWidget* MainWindow::createDiagnostics() {
  auto* result = panel();
  auto* layout = new QHBoxLayout(result);
  layout->setContentsMargins(10, 8, 10, 8);
  layout->setSpacing(12);
  auto* connections = new QVBoxLayout;
  connections->setSpacing(2);
  connections->addWidget(label(QStringLiteral("Communications")));
  const QStringList names{QStringLiteral("Enet Link"), QStringLiteral("Robot Radio"),
                          QStringLiteral("Robot"),     QStringLiteral("FMS"),
                          QStringLiteral("Enet"),      QStringLiteral("WiFi"),
                          QStringLiteral("USB"),       QStringLiteral("Firewall")};
  for (const auto& name : names) {
    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    auto* dot = label(QStringLiteral("●"));
    dot->setStyleSheet(QStringLiteral("color: #777;"));
    row->addWidget(dot);
    row->addWidget(label(name), 1);
    connections->addLayout(row);
  }
  connections->addStretch();
  layout->addLayout(connections, 2);
  auto* versions = new QVBoxLayout;
  versions->setSpacing(3);
  versions->addWidget(label(QStringLiteral("Versions & Info")));
  diagnostics_ = label({});
  diagnostics_->setObjectName(QStringLiteral("diagnostics"));
  diagnostics_->setWordWrap(true);
  diagnostics_->setStyleSheet(QStringLiteral("font-size: 12px;"));
  versions->addWidget(diagnostics_, 1);
  for (const auto& text :
       {QStringLiteral("Reboot roboRIO"), QStringLiteral("Restart Robot Code")}) {
    auto* button = new QPushButton(text);
    button->setEnabled(false);
    button->setToolTip(
        QStringLiteral("Requires robot communication, not implemented in this preview."));
    versions->addWidget(button);
  }
  layout->addLayout(versions, 3);
  result->setToolTip(QStringLiteral("Network interfaces and robot services are not queried."));
  return result;
}

QWidget* MainWindow::createSetup() {
  auto* result = panel();
  auto* layout = new QHBoxLayout(result);
  layout->setContentsMargins(10, 14, 10, 6);
  layout->setSpacing(16);
  auto* fieldsWidget = new QWidget;
  fieldsWidget->setFixedWidth(154);
  auto* fields = new QVBoxLayout(fieldsWidget);
  fields->setContentsMargins(0, 0, 0, 0);
  fields->setSpacing(2);
  fields->addWidget(label(QStringLiteral("Team Number")));
  teamInput_ = spinBox(preferences_.team, 65535, QStringLiteral("teamNumber"));
  teamInput_->setFixedWidth(112);
  teamInput_->setAlignment(Qt::AlignCenter);
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
  layout->addWidget(fieldsWidget);

  auto* timing = new QVBoxLayout;
  timing->setSpacing(1);
  auto* timingTitle = label(QStringLiteral("Practice Timing (s)"));
  timingTitle->setAlignment(Qt::AlignRight);
  timing->addWidget(timingTitle);
  auto* grid = new QGridLayout;
  grid->setVerticalSpacing(2);
  const QStringList stages{QStringLiteral("Countdown"), QStringLiteral("Autonomous"),
                           QStringLiteral("Delay"), QStringLiteral("Teleoperated"),
                           QStringLiteral("End Game")};
  for (int i = 0; i < 5; ++i) {
    auto* stage = label(stages[i]);
    stage->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(stage, i, 0);
    auto* input =
        spinBox(preferences_.practiceSeconds[i], 600, QStringLiteral("practice%1").arg(i));
    input->setFixedWidth(42);
    grid->addWidget(input, i, 1);
    connect(input, &QSpinBox::valueChanged, this, [this, i](int value) {
      preferences_.practiceSeconds[i] = value;
      disableOperation();
      session_.setPracticeTimings(preferences_.practiceSeconds);
      savePreferences();
    });
  }
  timing->addLayout(grid);
  auto* audio = new QToolButton;
  audio->setObjectName(QStringLiteral("audioControl"));
  audio->setIcon(style()->standardIcon(QStyle::SP_MediaVolumeMuted));
  audio->setFixedSize(24, 22);
  audio->setEnabled(false);
  audio->setToolTip(QStringLiteral("Practice audio is not implemented."));
  timing->addWidget(audio, 0, Qt::AlignRight);
  layout->addLayout(timing, 1);
  connect(teamInput_, &QSpinBox::valueChanged, this, [this](int team) {
    preferences_.team = team;
    discovery_.cancel();
    disableOperation();
    savePreferences();
    refresh();
  });
  connect(dashboard_, &QComboBox::currentIndexChanged, this, [this](int index) {
    preferences_.dashboard = index;
    savePreferences();
  });
  connect(gameData_, &QLineEdit::textChanged, this, [this](const QString& data) {
    if (!network_.setGameData(data))
      return;
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
  rescan->setObjectName(QStringLiteral("rescanControllers"));
  rescan->setToolTip(QStringLiteral("Rescan Linux controllers. Existing slots stay in place."));
  connect(rescan, &QPushButton::clicked, this, [this] {
    stopRumble();
    controllers_.rescan();
  });
  left->addWidget(rescan);
  layout->addLayout(left, 1);
  auto* right = new QHBoxLayout;
  right->setSpacing(9);
  auto* axesColumn = new QVBoxLayout;
  axesColumn->setSpacing(6);
  axesColumn->addWidget(label(QStringLiteral("Axes")));
  for (int i = 0; i < 6; ++i) {
    axes_[i] = new QProgressBar;
    axes_[i]->setObjectName(QStringLiteral("axis%1").arg(i));
    axes_[i]->setRange(0, 200);
    axes_[i]->setFixedSize(70, 14);
    axes_[i]->setAccessibleName(QStringLiteral("Axis %1").arg(i));
    axesColumn->addWidget(axes_[i]);
  }
  axesColumn->addStretch();
  right->addLayout(axesColumn);
  auto* buttonColumn = new QVBoxLayout;
  buttonColumn->setSpacing(2);
  buttonColumn->addWidget(label(QStringLiteral("Buttons")));
  for (int i = 0; i < 12; ++i) {
    buttons_[i] = label({});
    buttons_[i]->setObjectName(QStringLiteral("button%1").arg(i + 1));
    buttons_[i]->setAccessibleName(QStringLiteral("Button %1").arg(i + 1));
    buttons_[i]->setAlignment(Qt::AlignCenter);
    buttons_[i]->setFixedSize(13, 8);
    buttonColumn->addWidget(buttons_[i]);
  }
  buttonColumn->addStretch();
  right->addLayout(buttonColumn);
  auto* povColumn = new QVBoxLayout;
  povColumn->setSpacing(2);
  povColumn->addWidget(label(QStringLiteral("POV")));
  pov_ = new PovIndicator;
  pov_->setObjectName(QStringLiteral("pov"));
  povColumn->addWidget(pov_);
  povColumn->addStretch();
  povColumn->addWidget(label(QStringLiteral("Rumble")));
  for (int i = 0; i < 2; ++i) {
    rumble_[i] = new RumbleBar;
    rumble_[i]->setObjectName(i == 0 ? QStringLiteral("rumbleRight")
                                     : QStringLiteral("rumbleLeft"));
    rumble_[i]->setAccessibleName(i == 0 ? QStringLiteral("Right rumble")
                                         : QStringLiteral("Left rumble"));
    rumble_[i]->setToolTip(i == 0 ? QStringLiteral("Hold to test right (high frequency) rumble.")
                                  : QStringLiteral("Hold to test left (low frequency) rumble."));
    rumble_[i]->setEnabled(false);
    connect(rumble_[i], &RumbleBar::strengthChanged, this, &MainWindow::updateRumble);
    povColumn->addWidget(rumble_[i]);
  }
  devices_->setCurrentRow(0);
  connect(devices_, &QListWidget::currentRowChanged, this, [this] {
    stopRumble();
    refreshUsb();
  });
  right->addLayout(povColumn);
  layout->addLayout(right, 1);
  return result;
}

QWidget* MainWindow::createPower() {
  auto* result = panel();
  auto* layout = new QVBoxLayout(result);
  layout->setContentsMargins(12, 9, 12, 8);
  auto* grid = new QGridLayout;
  grid->setVerticalSpacing(9);
  const QStringList disable{QStringLiteral("Disable #"), QStringLiteral("Comms   —"),
                            QStringLiteral("12V        —"), QStringLiteral("5V          —")};
  const QStringList shorts{QStringLiteral("Short #"), QStringLiteral("6V     —"),
                           QStringLiteral("5V     —"), QStringLiteral("3.3V  —")};
  const QStringList can{
      QStringLiteral("CAN Metrics"),         QStringLiteral("Utilization %   —"),
      QStringLiteral("Bus Off           —"), QStringLiteral("TX Full            —"),
      QStringLiteral("Receive         —"),   QStringLiteral("Transmit        —")};
  for (int i = 0; i < 6; ++i) {
    if (i < 4) {
      grid->addWidget(label(disable[i]), i, 0);
      grid->addWidget(label(shorts[i]), i, 1);
    }
    grid->addWidget(label(can[i]), i, 2);
  }
  layout->addLayout(grid);
  layout->addStretch();
  result->setToolTip(QStringLiteral("No CAN or power telemetry received."));
  return result;
}

QWidget* MainWindow::createStatus() {
  auto* result = new QWidget;
  result->setFixedWidth(150);
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
    lamps_[i]->setObjectName(QStringLiteral("statusLamp%1").arg(i));
    lamps_[i]->setToolTip(i == 2 ? QStringLiteral("At least one readable Linux controller.")
                                 : QStringLiteral("Local robot simulation. No network data."));
    layout->addWidget(lamps_[i]);
  }
  layout->addStretch();
  status_ = label({});
  status_->setObjectName(QStringLiteral("robotStatus"));
  status_->setWordWrap(true);
  status_->setAlignment(Qt::AlignCenter);
  status_->setMinimumHeight(46);
  layout->addWidget(status_);
  dataSource_ = label(QStringLiteral("ALFA · EM TESTES"));
  dataSource_->setObjectName(QStringLiteral("dataSource"));
  dataSource_->setAlignment(Qt::AlignCenter);
  dataSource_->setToolTip(QStringLiteral(
      "Versão alfa de testes. Compatibilidade com NI 26.0/roboRIO físico ainda não validada."));
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
  auto* settings = new QToolButton;
  settings->setObjectName(QStringLiteral("consoleSettings"));
  settings->setIcon(symbolIcon(Symbol::Setup, QColor("#dddddd")));
  settings->setIconSize({15, 15});
  settings->setFixedSize(20, 20);
  settings->setToolTip(QStringLiteral("Console and local simulation settings (F10)"));
  header->addWidget(settings);
  header->addStretch();
  layout->addLayout(header);
  messages_ = new QPlainTextEdit;
  messages_->setObjectName(QStringLiteral("messages"));
  messages_->setReadOnly(true);
  messages_->setMaximumBlockCount(200);
  layout->addWidget(messages_, 1);
  controlsMenu_ = new QMenu(this);
  controlsMenu_->setObjectName(QStringLiteral("consoleMenu"));
  auto* clear = controlsMenu_->addAction(QStringLiteral("Clear messages"));
  clear->setObjectName(QStringLiteral("clearMessages"));
  connect(clear, &QAction::triggered, messages_, &QPlainTextEdit::clear);
  auto* robotConnection = controlsMenu_->addAction(QStringLiteral("Robot connection (UDP/TCP)..."));
  robotConnection->setObjectName(QStringLiteral("robotConnection"));
  connect(robotConnection, &QAction::triggered, this, &MainWindow::showRobotConnection);
  exportDiagnostics_ = controlsMenu_->addAction(QStringLiteral("Export network diagnostics..."));
  exportDiagnostics_->setObjectName(QStringLiteral("exportNetworkDiagnostics"));
  connect(exportDiagnostics_, &QAction::triggered, this, &MainWindow::exportDiagnostics);
  controlsMenu_->addSeparator();
  auto* controls = new QWidgetAction(controlsMenu_);
  controls->setDefaultWidget(createDemoControls());
  controlsMenu_->addAction(controls);
  connect(settings, &QToolButton::clicked, this, [this, settings] {
    controlsMenu_->popup(settings->mapToGlobal(QPoint(0, settings->height())));
  });
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
  auto* logs = new QPushButton(QStringLiteral("Logs ..."));
  logs->setFixedSize(64, 21);
  logs->setEnabled(false);
  logs->setToolTip(QStringLiteral("Robot log viewer is not implemented."));
  toolbar->addWidget(logs);
  toolbar->addStretch();
  auto* group = new QButtonGroup(this);
  const std::array<int, 3> windows{12, 60, 300};
  const QStringList captions{QStringLiteral("12 s"), QStringLiteral("1 m"), QStringLiteral("5 m")};
  for (int i = 0; i < 3; ++i) {
    auto* button = new QPushButton(captions[i]);
    button->setCheckable(true);
    button->setFixedSize(33, 21);
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

QWidget* MainWindow::createDemoControls() {
  auto* result = new QWidget;
  result->setFixedWidth(290);
  auto* layout = new QVBoxLayout(result);
  layout->setContentsMargins(10, 6, 10, 8);
  layout->setSpacing(8);
  footer_ = label(QStringLiteral("ALFA — VERSÃO DE TESTES · No robot commands"));
  footer_->setObjectName(QStringLiteral("footer"));
  footer_->setWordWrap(true);
  layout->addWidget(footer_);
  simulate_ = new QCheckBox(QStringLiteral("Simulate robot"));
  simulate_->setObjectName(QStringLiteral("simulateRobot"));
  simulate_->setToolTip(
      QStringLiteral("Local generated data only. No hardware or network is used."));
  layout->addWidget(simulate_);
  auto* stops = new QHBoxLayout;
  stops->addWidget(autoStop_);
  stops->addWidget(stop_);
  layout->addLayout(stops);
  reset_ = new QPushButton(QStringLiteral("Reset demo"));
  reset_->setObjectName(QStringLiteral("resetDemo"));
  reset_->setFixedHeight(21);
  layout->addWidget(reset_);
  connect(simulate_, &QCheckBox::toggled, this, [this](bool enabled) {
    if (enabled) {
      discovery_.cancel();
      network_.stop();
      usingNetwork_ = false;
      chart_->setSource(TelemetryChart::Source::Simulation);
      lastChartSample_ = -1;
      pressedKeys_.clear();
    }
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
  if (tab != 3)
    stopRumble();
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

void MainWindow::disableOperation() {
  session_.disable();
  network_.disable();
}

void MainWindow::emergencyStopOperation() {
  if (usingNetwork_)
    network_.emergencyStop();
  else
    session_.emergencyStop();
}

void MainWindow::exportDiagnostics() {
  if (!usingNetwork_)
    return;
  disableOperation();
  stopRumble();
  pressedKeys_.clear();
  const auto directory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
  const auto name = QStringLiteral("DriveStation-diagnostics-%1.json")
                        .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss")));
  QFileDialog dialog(this, QStringLiteral("Export network diagnostics"), directory,
                       QStringLiteral("DriveStation diagnostics (*.json)"));
  dialog.setObjectName(QStringLiteral("exportDiagnosticsDialog"));
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  dialog.setDefaultSuffix(QStringLiteral("json"));
  dialog.selectFile(name);
  if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
    return;
  QString error;
  if (!network_.saveDiagnostics(dialog.selectedFiles().first(), error))
    QMessageBox::warning(this, QStringLiteral("Diagnostics not saved"), error);
  else
    appendMessage(QStringLiteral("Network diagnostics exported. Enable remains explicit."));
}

void MainWindow::showRobotConnection() {
  disableOperation();
  stopRumble();
  pressedKeys_.clear();
  if (!connectionDialog_) {
    connectionDialog_ = new QDialog(this);
    connectionDialog_->setObjectName(QStringLiteral("robotConnectionDialog"));
    connectionDialog_->setMinimumWidth(520);
    connectionDialog_->setWindowTitle(stationTitle(QStringLiteral("Robot connection")));
    auto* layout = new QVBoxLayout(connectionDialog_);
    auto* description = label(QStringLiteral(
        "Experimental roboRIO UDP/TCP. Connect sends inputs, initially disabled.\n"
        "Find resolves a team hostname; choosing an address does not connect. Practice is local.\n"
        "Physical compatibility is unverified."));
    description->setWordWrap(true);
    layout->addWidget(description);
    auto* fields = new QFormLayout;
    robotAddress_ = new QLineEdit(QStringLiteral("127.0.0.1"));
    robotAddress_->setObjectName(QStringLiteral("robotAddress"));
    robotAddress_->setMaxLength(15);
    fields->addRow(QStringLiteral("Robot IPv4"), robotAddress_);
    robotPort_ = spinBox(wire::RobotPort, 65535, QStringLiteral("robotPort"));
    robotPort_->setMinimum(1);
    fields->addRow(QStringLiteral("Robot UDP port"), robotPort_);
    localPort_ = spinBox(wire::StationPort, 65535, QStringLiteral("localPort"));
    localPort_->setToolTip(QStringLiteral("0 chooses an ephemeral port for local tests."));
    fields->addRow(QStringLiteral("Local UDP port"), localPort_);
    useTcp_ = new QCheckBox(QStringLiteral("Descriptors and robot console"));
    useTcp_->setObjectName(QStringLiteral("useTcp"));
    useTcp_->setChecked(true);
    fields->addRow(QStringLiteral("Use TCP"), useTcp_);
    tcpPort_ = spinBox(tcp::RobotPort, 65535, QStringLiteral("tcpPort"));
    tcpPort_->setMinimum(1);
    fields->addRow(QStringLiteral("Robot TCP port"), tcpPort_);
    connect(useTcp_, &QCheckBox::toggled, this, [this](bool enabled) {
      tcpPort_->setEnabled(enabled && !network_.link().connection().active());
    });
    layout->addLayout(fields);
    auto* discoveryRow = new QHBoxLayout;
    discoveryRow->addWidget(label(QStringLiteral("Discovery team")));
    discoveryTeam_ = spinBox(preferences_.team, 65535, QStringLiteral("discoveryTeam"));
    discoveryTeam_->setFixedWidth(70);
    discoveryRow->addWidget(discoveryTeam_);
    findRobot_ = new QPushButton(QStringLiteral("Find roboRIO"));
    findRobot_->setObjectName(QStringLiteral("findRobot"));
    findRobot_->setAutoDefault(false);
    cancelDiscovery_ = new QPushButton(QStringLiteral("Cancel search"));
    cancelDiscovery_->setObjectName(QStringLiteral("cancelDiscovery"));
    cancelDiscovery_->setAutoDefault(false);
    discoveryRow->addWidget(findRobot_);
    discoveryRow->addWidget(cancelDiscovery_);
    layout->addLayout(discoveryRow);
    discoveryResults_ = new QListWidget;
    discoveryResults_->setObjectName(QStringLiteral("discoveryResults"));
    discoveryResults_->setFixedHeight(85);
    layout->addWidget(discoveryResults_);
    useDiscovery_ = new QPushButton(QStringLiteral("Use selected address"));
    useDiscovery_->setObjectName(QStringLiteral("useDiscoveryAddress"));
    useDiscovery_->setAutoDefault(false);
    layout->addWidget(useDiscovery_);
    discoveryStatus_ = label({});
    discoveryStatus_->setObjectName(QStringLiteral("discoveryStatus"));
    discoveryStatus_->setWordWrap(true);
    layout->addWidget(discoveryStatus_);
    connect(discoveryTeam_, &QSpinBox::valueChanged, teamInput_, &QSpinBox::setValue);
    connect(findRobot_, &QPushButton::clicked, this, [this] {
      if (network_.link().connection().active())
        return;
      disableOperation();
      pressedKeys_.clear();
      discovery_.start(preferences_.team);
    });
    connect(cancelDiscovery_, &QPushButton::clicked, this, [this] { discovery_.cancel(); });
    connect(discoveryResults_, &QListWidget::currentRowChanged, this, [this] {
      useDiscovery_->setEnabled(discoveryResults_->currentItem() &&
                                !network_.link().connection().active());
    });
    connect(useDiscovery_, &QPushButton::clicked, this, [this] {
      if (network_.link().connection().active() || !discoveryResults_->currentItem())
        return;
      const auto address = discoveryResults_->currentItem()->data(Qt::UserRole).toString();
      if (discoveryResults_->currentItem()->data(Qt::UserRole + 1).toBool() &&
          !discovery_.addresses().contains(QHostAddress(address))) {
        refreshDiscovery();
        return;
      }
      disableOperation();
      discovery_.cancel();
      pressedKeys_.clear();
      robotAddress_->setText(address);
    });
    connect(robotAddress_, &QLineEdit::textChanged, this, [this] {
      if (discovery_.active() || !discovery_.addresses().isEmpty())
        discovery_.cancel();
    });
    connectionStatus_ = label(QStringLiteral("Disconnected. No network traffic until Connect."));
    connectionStatus_->setObjectName(QStringLiteral("connectionStatus"));
    connectionStatus_->setWordWrap(true);
    layout->addWidget(connectionStatus_);
    auto* actions = new QHBoxLayout;
    connectRobot_ = new QPushButton(QStringLiteral("Connect"));
    connectRobot_->setObjectName(QStringLiteral("connectRobot"));
    disconnectRobot_ = new QPushButton(QStringLiteral("Disconnect"));
    disconnectRobot_->setObjectName(QStringLiteral("disconnectRobot"));
    auto* done = new QPushButton(QStringLiteral("Close"));
    done->setObjectName(QStringLiteral("closeRobotConnection"));
    for (auto* button : {connectRobot_, disconnectRobot_, done}) {
      button->setAutoDefault(false);
      actions->addWidget(button);
    }
    layout->addLayout(actions);
    connect(done, &QPushButton::clicked, connectionDialog_, &QDialog::hide);
    connect(disconnectRobot_, &QPushButton::clicked, this, [this] {
      discovery_.cancel();
      network_.stop();
      pressedKeys_.clear();
      appendMessage(QStringLiteral("UDP disconnected. Final Disable sent on a best-effort basis."));
      refresh();
    });
    connect(connectRobot_, &QPushButton::clicked, this, [this] {
      discovery_.cancel();
      const QHostAddress address(robotAddress_->text().trimmed());
      disableOperation();
      session_.setConnected(false);
      usingNetwork_ = true;
      pressedKeys_.clear();
      chart_->setSource(TelemetryChart::Source::Network);
      lastChartSample_ = -1;
      network_.setMode(session_.mode() == DemoSession::Mode::Autonomous ? wire::Mode::Autonomous
                       : session_.mode() == DemoSession::Mode::Test     ? wire::Mode::Test
                                                                        : wire::Mode::Teleoperated);
      const bool started =
          network_.start(address, static_cast<quint16>(robotPort_->value()),
                         static_cast<quint16>(localPort_->value()),
                         useTcp_->isChecked() ? static_cast<quint16>(tcpPort_->value()) : 0);
      if (started)
        appendMessage(QStringLiteral("UDP started: %1:%2, local port %3. Enable is explicit.")
                          .arg(address.toString())
                          .arg(robotPort_->value())
                          .arg(network_.link().localPort()));
      refresh();
    });
  }
  const bool active = network_.link().connection().active();
  robotAddress_->setEnabled(!active);
  robotPort_->setEnabled(!active);
  localPort_->setEnabled(!active);
  useTcp_->setEnabled(!active);
  tcpPort_->setEnabled(!active && useTcp_->isChecked());
  connectRobot_->setEnabled(!active);
  disconnectRobot_->setEnabled(active);
  refreshDiscovery();
  connectionDialog_->show();
  connectionDialog_->raise();
  connectionDialog_->activateWindow();
}

void MainWindow::refreshDiscovery() {
  if (!discoveryResults_)
    return;
  const bool connected = network_.link().connection().active();
  const auto selected = discoveryResults_->currentItem()
                            ? discoveryResults_->currentItem()->data(Qt::UserRole).toString()
                            : QString();
  const bool selectedResolved = discoveryResults_->currentItem() &&
                                discoveryResults_->currentItem()->data(Qt::UserRole + 1).toBool();
  const QSignalBlocker listBlocker(discoveryResults_);
  const QSignalBlocker teamBlocker(discoveryTeam_);
  discoveryTeam_->setValue(preferences_.team);
  discoveryResults_->clear();
  QStringList present;
  const auto add = [&](const QHostAddress& address, const QString& origin, bool resolved) {
    const auto text = address.toString();
    if (present.contains(text))
      return;
    present.append(text);
    auto* item =
        new QListWidgetItem(QStringLiteral("%1 · %2").arg(text, origin), discoveryResults_);
    item->setData(Qt::UserRole, text);
    item->setData(Qt::UserRole + 1, resolved);
    if (text == selected && resolved == selectedResolved)
      discoveryResults_->setCurrentItem(item);
  };
  for (const auto& address : discovery_.addresses())
    add(address, QStringLiteral("mDNS response · robot identity unverified"), true);
  add(QHostAddress(QStringLiteral("172.22.11.2")), QStringLiteral("USB hint · not checked"), false);
  if (const auto address = discovery::teamAddress(preferences_.team))
    add(*address, QStringLiteral("TE.AM hint · not checked"), false);
  discoveryResults_->setEnabled(!connected);
  discoveryTeam_->setEnabled(!connected);
  findRobot_->setEnabled(!connected && !discovery_.active() && preferences_.team > 0);
  findRobot_->setToolTip(QStringLiteral("mDNS one-shot lookup for %1; set a team number first.")
                             .arg(discovery::robotHostname(preferences_.team)));
  cancelDiscovery_->setEnabled(discovery_.active() || !discovery_.addresses().isEmpty());
  useDiscovery_->setEnabled(!connected && discoveryResults_->currentItem());
  discoveryStatus_->setText(discovery_.statusText());
}

void MainWindow::refreshNetwork() {
  const auto& link = network_.link();
  const auto& connection = link.connection();
  const auto& status = connection.status();
  const bool connected = connection.connected();
  const bool requested = connection.enableRequested();
  setWindowTitle(stationTitle(QStringLiteral("Experimental network")));
  const QSignalBlocker blocker(simulate_);
  simulate_->setChecked(false);
  team_->setText(QString::number(preferences_.team));
  voltage_->setText(status ? QStringLiteral("%1 V").arg(status->voltage(), 0, 'f', 2)
                           : QStringLiteral("--.--"));
  battery_->setVoltage(status ? status->voltage() : 0, connected);
  lamps_[0]->setLit(connected);
  lamps_[1]->setLit(status && status->hasRobotCode() && !status->codeInitializing);
  for (int i = 0; i < 2; ++i)
    lamps_[i]->setToolTip(QStringLiteral("State received from the configured UDP peer."));
  status_->setText(network_.statusText());
  status_->setStyleSheet(connection.emergencyStopped()
                             ? QStringLiteral("color: #ff625a; font-weight: bold;")
                             : QStringLiteral("font-weight: bold;"));
  const int mode = network_.mode() == wire::Mode::Autonomous ? 1
                   : network_.mode() == wire::Mode::Test     ? 3
                                                             : 0;
  modeButtons_[mode]->setChecked(true);
  modeButtons_[2]->setToolTip(QStringLiteral("Practice is available only in the local demo."));
  enable_->setEnabled(network_.canEnable());
  enable_->setChecked(network_.enabled());
  enable_->setToolTip(QStringLiteral("Request UDP Enable. Shortcut: hold [ ] \\."));
  disable_->setEnabled(connection.active());
  disable_->setChecked(!requested && !network_.enabled());
  stop_->setEnabled(connection.active() && !connection.emergencyStopped());
  autoStop_->setEnabled(false);
  reset_->setEnabled(false);
  elapsed_->setText(formatTime(network_.elapsedMs()));
  elapsed_->setToolTip(
      QStringLiteral("Local time since UDP Enable request; not robot match time."));
  dataSource_->setVisible(true);
  dataSource_->setText(settingsError_       ? QStringLiteral("ALFA · PREFERENCES ERROR")
                       : network_.usesTcp() ? QStringLiteral("ALFA · TESTES · UDP/TCP")
                                            : QStringLiteral("ALFA · TESTES · UDP"));
  footer_->setText(
      QStringLiteral("Network sends robot commands. E-Stop remains latched until exit."));
  QString rio = QStringLiteral("Not received"), library = QStringLiteral("Not received");
  for (const auto& version : network_.tcpLink().versions()) {
    if (version.deviceType != 0)
      continue;
    if (version.name == QStringLiteral("roboRIO image"))
      rio = version.version.left(24);
    if (version.name == QStringLiteral("FRC_Lib_Version"))
      library = version.version.left(24);
  }
  diagnostics_->setText(QStringLiteral("DS: %1 (UDP)\nTX: %2  RX: %3  Rejected: %4\n"
                                       "RTT: %5 ms\nTCP: %6\nRIO: %7\nLib: %8")
                            .arg(DRIVESTATION_VERSION)
                            .arg(link.sentCount())
                            .arg(link.receivedCount())
                            .arg(link.rejectedCount())
                            .arg(connection.roundTripMs()
                                     ? QString::number(*connection.roundTripMs())
                                     : QStringLiteral("—"))
                            .arg(!network_.usesTcp()          ? QStringLiteral("Off")
                                 : network_.tcpLink().ready() ? QStringLiteral("Synchronized")
                                                              : QStringLiteral("Not synchronized"))
                            .arg(rio, library));
  if (connectionDialog_) {
    const bool active = connection.active();
    robotAddress_->setEnabled(!active);
    robotPort_->setEnabled(!active);
    localPort_->setEnabled(!active);
    useTcp_->setEnabled(!active);
    tcpPort_->setEnabled(!active && useTcp_->isChecked());
    connectRobot_->setEnabled(!active);
    disconnectRobot_->setEnabled(active);
    connectionStatus_->setText(
        QStringLiteral("%1 · local port %2 · UDP TX %3 / RX %4\nTCP TX %5 / RX %6\n%7")
            .arg(network_.statusText())
            .arg(link.localPort())
            .arg(link.sentCount())
            .arg(link.receivedCount())
            .arg(network_.tcpLink().sentCount())
            .arg(network_.tcpLink().receivedCount())
            .arg(!network_.inputError().isEmpty() ? network_.inputError()
                 : !link.lastError().isEmpty()    ? link.lastError()
                                                  : network_.tcpLink().lastError()));
  }
  refreshUsb();
}

void MainWindow::requestEnable() {
  if (usingNetwork_) {
    if (network_.enable())
      appendMessage(QStringLiteral("UDP Enable requested; waiting for robot status."));
    return;
  }
  if (session_.enable()) {
    tickClock_.restart();
    appendMessage(session_.mode() == DemoSession::Mode::Practice
                      ? QStringLiteral("Practice started locally. No commands sent.")
                      : QStringLiteral("Demo enabled. No commands sent."));
  }
}

void MainWindow::refresh() {
  exportDiagnostics_->setEnabled(usingNetwork_);
  refreshDiscovery();
  modeButtons_[2]->setEnabled(!usingNetwork_);
  simulate_->setEnabled(!network_.link().connection().active());
  reset_->setEnabled(!usingNetwork_);
  if (usingNetwork_) {
    refreshNetwork();
    return;
  }
  setWindowTitle(stationTitle(QStringLiteral("Offline preview")));
  const bool connected = session_.connected();
  modeButtons_[2]->setToolTip(QStringLiteral("Practice sequence in the local demo."));
  for (int i = 0; i < 2; ++i)
    lamps_[i]->setToolTip(QStringLiteral("Local robot simulation. No network data."));
  const QSignalBlocker blocker(simulate_);
  simulate_->setChecked(connected);
  team_->setText(QString::number(preferences_.team));
  voltage_->setText(connected ? QStringLiteral("%1 V").arg(session_.voltage(), 0, 'f', 2)
                              : QStringLiteral("--.--"));
  battery_->setVoltage(session_.voltage(), connected);
  lamps_[0]->setLit(connected);
  lamps_[1]->setLit(connected);
  status_->setText(session_.status());
  const bool practice = session_.mode() == DemoSession::Mode::Practice;
  const bool pending = practice && connected && session_.practice().pendingEnable();
  dataSource_->setText(!connected ? QStringLiteral("ALFA · EM TESTES")
                       : pending  ? QStringLiteral("ALFA · SIM · WILL ENABLE")
                                  : QStringLiteral("ALFA · SIMULATED"));
  const bool flash = pending && (static_cast<qint64>(session_.simulationSeconds() * 2) % 2 == 0);
  status_->setStyleSheet(session_.emergencyStopped()
                             ? QStringLiteral("color: #ff625a; font-weight: bold;")
                         : flash ? QStringLiteral("background: #a35900; font-weight: bold;")
                                 : QStringLiteral("font-weight: bold;"));
  modeButtons_[static_cast<int>(session_.mode())]->setChecked(true);
  enable_->setEnabled(session_.canEnable());
  enable_->setChecked(session_.enabled());
  disable_->setChecked(!session_.enabled());
  enable_->setToolTip(
      session_.mode() == DemoSession::Mode::Practice
          ? QStringLiteral("Start the local Practice sequence using the times in Setup.")
          : QStringLiteral("Enable the local demonstration. Shortcut: hold [ ] \\."));
  disable_->setEnabled(session_.busy());
  stop_->setEnabled(connected && !session_.emergencyStopped());
  autoStop_->setEnabled(practice && session_.enabled() &&
                        session_.practice().phase() == PracticeSequence::Phase::Autonomous);
  reset_->setEnabled(connected || session_.emergencyStopped() || session_.elapsedMs() != 0);
  elapsedCaption_->setText(QStringLiteral("Elapsed Time"));
  elapsed_->setText(formatTime(session_.elapsedMs()));
  elapsed_->setToolTip(
      practice && session_.practice().active()
          ? QStringLiteral("Local Practice elapsed time (includes waits). Phase left: %1")
                .arg(formatTime(session_.practice().remainingMs()))
          : QStringLiteral("Elapsed time in the local demonstration."));
  dataSource_->setVisible(true);
  if (settingsError_)
    dataSource_->setText(QStringLiteral("ALFA · PREFERENCES ERROR"));
  footer_->setText(settingsError_ ? QStringLiteral("Preferences not saved. Check file permissions.")
                                  : QStringLiteral("Local simulation only. No robot commands."));
  if (session_.practice().phase() != lastPracticePhase_) {
    lastPracticePhase_ = session_.practice().phase();
    appendMessage(
        QStringLiteral("Practice: %1 (local).").arg(DemoSession::phaseName(lastPracticePhase_)));
  }
  if (session_.practice().autoStopped() && !lastAutoStopped_) {
    appendMessage(QStringLiteral("A-Stop: Autonomous disabled; Teleop will resume automatically."));
  }
  lastAutoStopped_ = session_.practice().autoStopped();
  diagnostics_->setText(QStringLiteral("DS: %2 (offline preview)\n"
                                       "Robot: %1\nRIO: Not received\nLib: Not received\n"
                                       "Free RAM (MB): —")
                            .arg(connected ? QStringLiteral("Simulated (no network)")
                                           : QStringLiteral("Not connected"),
                                 DRIVESTATION_VERSION));
  refreshUsb();
}

void MainWindow::refreshUsb() {
  const auto& snapshot = controllers_.controllerSlots();
  lamps_[2]->setLit(controllers_.anyReadable());
  for (int i = 0; i < ControllerHub::SlotCount; ++i) {
    const auto& slot = snapshot[i];
    auto* item = devices_->item(i);
    item->setText(
        QStringLiteral("%1  %2").arg(i).arg(slot.device ? slot.device->name : QStringLiteral("—")));
    item->setForeground(slot.reading ? QColor("#72e22d") : QColor("#bcbcbc"));
    item->setToolTip(slot.error.isEmpty() ? item->text() : slot.error);
  }
  const int selected = devices_->currentRow();
  const auto* reading =
      selected >= 0 && selected < ControllerHub::SlotCount && snapshot[selected].reading
          ? &*snapshot[selected].reading
          : nullptr;
  const QString summary =
      reading ? QStringLiteral("Linux inputs: %1 axes, %2 buttons, %3 POVs. Displays axes 0–5, "
                               "buttons 1–12 and POV 0.")
                    .arg(reading->axes.size())
                    .arg(reading->buttons.size())
                    .arg(reading->povs.size())
              : QStringLiteral(
                    "No readable controller selected. Robot simulation does not create USB input.");
  devices_->setToolTip(summary + QStringLiteral(" Extra devices outside the six slots: %1.")
                                     .arg(controllers_.unassignedCount()));
  for (int i = 0; i < 6; ++i) {
    const bool available = reading && i < reading->axes.size();
    const double value = available ? normalizedAxis(reading->axes[i]) : 0;
    axes_[i]->setValue(available ? qRound((value + 1) * 100) : 0);
    axes_[i]->setFormat(available ? QString::number(value, 'f', 2) : QStringLiteral("—"));
    axes_[i]->setAccessibleDescription(available ? QString::number(value, 'f', 4)
                                                 : QStringLiteral("Unavailable"));
  }
  for (int i = 0; i < 12; ++i) {
    const bool available = reading && i < reading->buttons.size();
    const bool pressed = available && reading->buttons[i];
    buttons_[i]->setStyleSheet(pressed ? QStringLiteral("background: #56bb1d; color: #111;")
                                       : QStringLiteral("background: #303030; color: #888;"));
    buttons_[i]->setAccessibleDescription(!available ? QStringLiteral("Unavailable")
                                          : pressed  ? QStringLiteral("Pressed")
                                                     : QStringLiteral("Released"));
  }
  pov_->setAngle(reading && !reading->povs.isEmpty() ? std::optional<int>(reading->povs[0])
                                                     : std::nullopt);
  for (auto* bar : rumble_)
    bar->setEnabled(reading && reading->supportsRumble);
}

void MainWindow::stopRumble() {
  if (stoppingRumble_)
    return;
  stoppingRumble_ = true;
  for (auto* bar : rumble_)
    if (bar)
      bar->cancel();
  controllers_.stopRumble();
  stoppingRumble_ = false;
}

void MainWindow::updateRumble() {
  if (stoppingRumble_)
    return;
  if (pages_->currentIndex() != 3 || !isVisible() ||
      !controllers_.rumble(devices_->currentRow(), rumble_[1]->strength(), rumble_[0]->strength()))
    stopRumble();
}

void MainWindow::tick() {
  const qint64 elapsed = tickClock_.restart();
  const bool cancelled = elapsed > DemoSession::MaxTickGapMs && session_.busy();
  session_.advance(elapsed);
  if (cancelled)
    appendMessage(
        QStringLiteral("Demo cancelled after delayed update; enable explicitly to restart."));
  if (!usingNetwork_ && session_.connected() &&
      session_.simulationSeconds() - lastChartSample_ >= 0.2) {
    chart_->appendSample(session_.simulationSeconds(), session_.tripTime(), session_.voltage());
    lastChartSample_ = session_.simulationSeconds();
  }
  if (usingNetwork_)
    chart_->advanceTime(network_.link().elapsedMs() / 1000.0);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::Hide && (watched == this || watched == connectionDialog_))
    discovery_.cancel();
  if (event->type() == QEvent::Hide && watched == this) {
    stopRumble();
    pressedKeys_.clear();
    disableOperation();
  }
  if (event->type() == QEvent::ApplicationDeactivate ||
      (event->type() == QEvent::WindowDeactivate && watched == this &&
       !QApplication::activePopupWidget())) {
    stopRumble();
    pressedKeys_.clear();
    disableOperation();
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
  if (key->key() == Qt::Key_F1) {
    stopRumble();
    controllers_.rescan();
    return true;
  }
  const bool editing = qobject_cast<QLineEdit*>(widget) || qobject_cast<QSpinBox*>(widget);
  if (key->key() == Qt::Key_Backspace && !editing && !usingNetwork_ && session_.autonomousStop()) {
    pressedKeys_.clear();
    return true;
  }
  if (key->key() == Qt::Key_Space &&
      (usingNetwork_ ? network_.link().connection().active() : session_.connected())) {
    pressedKeys_.clear();
    emergencyStopOperation();
    return true;
  }
  if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
    pressedKeys_.clear();
    disableOperation();
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
    QWidget { color: #ededed; font-family: "Arial", "Liberation Sans", "Sans Serif"; font-size: 13px; }
    QWidget#stationWindow { background: #262626; }
    QDialog#robotConnectionDialog { background: #444444; }
    QFrame#panel, QWidget#messagePanel, QWidget#chartPanel {
      background: #444444; border: 1px solid #242424; border-radius: 4px;
    }
    QLabel, QCheckBox { background: transparent; border: none; }
    QLabel[muted="true"] { color: #b8b8b8; font-size: 10px; }
    QLabel#robotStatus { font-size: 16px; }
    QLabel#dataSource { color: #b8b8b8; font-size: 9px; font-weight: bold; }
    QLabel#footer { color: #bbb; font-size: 9px; }
    QLabel#elapsed, QLabel#voltage, QLabel#teamStatus { font-size: 18px; }
    QToolButton[navigation="true"] {
      background: #333333; border: 1px solid #292929; border-radius: 0; padding: 1px;
    }
    QToolButton[navigation="true"]:checked { border-right: 3px solid #00c9e8; background: #474747; }
    QToolButton[navigation="true"][rightSide="true"]:checked { border-right: 1px solid #292929; border-left: 3px solid #00c9e8; }
    QToolButton#closeStation { color: #bdbdbd; font-size: 29px; background: #541414; border: none; }
    QToolButton#consoleSettings, QToolButton#audioControl { background: transparent; border: none; }
    QToolButton[windowControl="true"], QToolButton[windowControl="true"]:disabled {
      background: #333; color: #888; border: 1px solid #2d2d2d; font-size: 20px;
    }
    QToolButton:hover, QPushButton:hover { background: #555; }
    QToolButton:focus, QPushButton:focus { border: 1px solid #888; }
    QPushButton { background: #393939; border: 1px solid #282828; padding: 3px 5px; }
    QPushButton:pressed, QPushButton:checked { background: #252525; border: 1px solid #888; }
    QPushButton:disabled { color: #858585; }
    QFrame#commandPanel { background: #303030; border: 1px solid #292929; }
    QPushButton#enable, QPushButton#disable { border: 1px solid #292929; padding: 0; background: #444; font-weight: bold; font-size: 17px; }
    QPushButton#enable:checked, QPushButton#disable:checked { background: #2b2b2b; }
    QPushButton#enable { color: #25bd00; }
    QPushButton#enable:disabled { color: #074700; }
    QPushButton#disable, QPushButton#disable:disabled { color: #ff1710; }
    QPushButton[modeSelector="true"] { padding: 0 10px; text-align: left; background: #444; border: 1px solid #303030; }
    QPushButton[modeSelector="true"]:checked { background: #303030; border: 1px solid #292929; }
    QPushButton[modeSelector="true"]:focus { border: 1px solid #888; }
    QPushButton#emergencyStop { color: #ff625a; }
    QLineEdit, QComboBox { color: #111; background: #ccc; border: 1px solid #333; padding: 2px 4px; min-height: 16px; }
    QLineEdit, QPlainTextEdit, QListWidget { selection-background-color: #666; selection-color: #fff; }
    QSpinBox { color: #111; background: #ccc; min-height: 20px; }
    QSpinBox QLineEdit { border: none; padding: 0; min-height: 0; background: transparent; }
    QComboBox QAbstractItemView { color: #eee; background: #444; selection-background-color: #666; }
    QComboBox#station { color: #eee; background: #444; padding: 1px 5px; }
    QComboBox#dashboard { color: #eee; background: #303030; }
    QListWidget { background: #484848; border: 1px solid #666; font-size: 13px; }
    QListWidget::item { padding: 1px 2px; }
    QPlainTextEdit { background: #2c2c2c; color: #ddd; border: none; font-family: "DejaVu Sans Mono", monospace; font-size: 10px; }
    QProgressBar { background: #303030; border: 1px solid #222; text-align: center; font-size: 9px; }
    QProgressBar::chunk { background: #26761b; }
    QProgressBar[hostMetric="true"] { background: #333; border: 1px solid #222; }
    QCheckBox { spacing: 4px; font-size: 10px; }
    QCheckBox::indicator { width: 11px; height: 11px; background: #555; border: 1px solid #999; }
    QCheckBox::indicator:checked { background: #bbb; border: 1px solid #ddd; }
    QToolTip { color: #eee; background: #242424; border: 1px solid #666; }
    QMenu { background: #444; color: #eee; border: 1px solid #777; }
  )"));
}

} // namespace ds
