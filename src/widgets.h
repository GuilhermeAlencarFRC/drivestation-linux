#pragma once

#include <QColor>
#include <QIcon>
#include <QVector>
#include <QWidget>

namespace ds {

enum class Symbol { Operation, Diagnostics, Setup, Usb, Power, Messages, Charts, Both };
QIcon symbolIcon(Symbol symbol, const QColor& color = QColor("#999999"));

class Lamp : public QWidget {
public:
  explicit Lamp(QString text, QWidget* parent = nullptr);
  void setLit(bool lit);
  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent*) override;

private:
  QString text_;
  bool lit_ = false;
};

class Battery : public QWidget {
public:
  explicit Battery(QWidget* parent = nullptr);
  void setVoltage(double voltage, bool connected);
  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent*) override;

private:
  double voltage_ = 0;
  bool connected_ = false;
};

class PovIndicator : public QWidget {
public:
  explicit PovIndicator(QWidget* parent = nullptr);

protected:
  void paintEvent(QPaintEvent*) override;
};

class TelemetryChart : public QWidget {
public:
  explicit TelemetryChart(QWidget* parent = nullptr);
  void appendSample(double seconds, double trip, double voltage);
  void clear();
  void setWindowSeconds(int seconds);

protected:
  void paintEvent(QPaintEvent*) override;

private:
  struct Sample {
    double seconds;
    double trip;
    double voltage;
  };
  QVector<Sample> samples_;
  int windowSeconds_ = 12;
};

} // namespace ds
