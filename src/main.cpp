#include "main_window.h"

#include <QApplication>
#include <QCheckBox>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QStyleFactory>
#include <QTimer>
#include <cstdio>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("drivestation-linux"));
  QApplication::setOrganizationName(QStringLiteral("DriveStationLinux"));
  QApplication::setApplicationVersion(QStringLiteral(DRIVESTATION_VERSION));
  QApplication::setDesktopFileName(
      QStringLiteral("io.github.GuilhermeAlencarFRC.DriveStationLinux"));
  QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("DriveStation Linux — ALFA / VERSÃO DE TESTES. "
                     "Local demo and explicit experimental roboRIO UDP/TCP."));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOption({QStringLiteral("config"),
                    QStringLiteral("Use a separate preferences INI file."),
                    QStringLiteral("file")});
  parser.addOption(
      {QStringLiteral("demo"), QStringLiteral("Explicitly connect the local simulated robot.")});
  parser.addOption({QStringLiteral("tab"),
                    QStringLiteral("Initial tab: operation, diagnostics, setup, usb, power."),
                    QStringLiteral("name"), QStringLiteral("operation")});
  parser.addOption({QStringLiteral("view"), QStringLiteral("Right pane: messages, charts, both."),
                    QStringLiteral("name"), QStringLiteral("messages")});
  parser.addOption({QStringLiteral("screenshot"),
                    QStringLiteral("Save the preview to a PNG file and exit."),
                    QStringLiteral("file")});
  parser.process(app);
  const QStringList tabs{QStringLiteral("operation"), QStringLiteral("diagnostics"),
                         QStringLiteral("setup"), QStringLiteral("usb"), QStringLiteral("power")};
  const QStringList views{QStringLiteral("messages"), QStringLiteral("charts"),
                          QStringLiteral("both")};
  if (!tabs.contains(parser.value(QStringLiteral("tab"))) ||
      !views.contains(parser.value(QStringLiteral("view")))) {
    std::fprintf(stderr, "Invalid tab or view. Run with --help for available values.\n");
    return 2;
  }
  if (parser.isSet(QStringLiteral("config")) && parser.value(QStringLiteral("config")).isEmpty()) {
    std::fprintf(stderr, "The configuration file path must not be empty.\n");
    return 2;
  }
  if (parser.isSet(QStringLiteral("screenshot"))) {
    const auto path = parser.value(QStringLiteral("screenshot"));
    if (path.isEmpty() || QFileInfo::exists(path)) {
      std::fprintf(stderr, "Screenshot requires a new file path; existing files are preserved.\n");
      return 2;
    }
  }

  ds::MainWindow window(ds::PreferenceStore(parser.value(QStringLiteral("config"))));
  window.selectTab(tabs.indexOf(parser.value(QStringLiteral("tab"))));
  window.selectRightView(views.indexOf(parser.value(QStringLiteral("view"))));
  if (parser.isSet(QStringLiteral("demo"))) {
    window.findChild<QCheckBox*>(QStringLiteral("simulateRobot"))->setChecked(true);
  }
  window.show();
  if (parser.isSet(QStringLiteral("screenshot"))) {
    const auto path = parser.value(QStringLiteral("screenshot"));
    QTimer::singleShot(350, &window, [&window, path] {
      if (!window.grab().save(path, "PNG")) {
        std::fprintf(stderr, "Could not save screenshot.\n");
        QApplication::exit(1);
      } else {
        QApplication::quit();
      }
    });
  }
  return app.exec();
}
