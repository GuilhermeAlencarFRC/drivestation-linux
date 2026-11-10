#include "widgets.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <utility>

namespace ds {

QIcon symbolIcon(Symbol symbol, const QColor& color) {
  QPixmap pixmap(48, 48);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  p.scale(2, 2);
  p.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  switch (symbol) {
  case Symbol::Operation:
    p.drawEllipse(QPointF(12, 12), 9, 9);
    p.drawEllipse(QPointF(12, 12), 2.5, 2.5);
    p.drawLine(QPointF(3, 10), QPointF(10, 11));
    p.drawLine(QPointF(14, 11), QPointF(21, 10));
    p.drawLine(QPointF(12, 15), QPointF(12, 21));
    break;
  case Symbol::Diagnostics:
    p.drawRoundedRect(QRectF(5, 3, 14, 15), 1, 1);
    p.drawEllipse(QPointF(12, 10), 4, 4);
    p.drawLine(QPointF(12, 10), QPointF(15, 7));
    p.drawLine(QPointF(9, 19), QPointF(9, 22));
    p.drawLine(QPointF(15, 19), QPointF(15, 22));
    break;
  case Symbol::Setup:
    p.drawEllipse(QPointF(12, 12), 6, 6);
    p.drawEllipse(QPointF(12, 12), 2, 2);
    for (int i = 0; i < 8; ++i) {
      p.save();
      p.translate(12, 12);
      p.rotate(i * 45);
      p.drawLine(QPointF(0, 6), QPointF(0, 9));
      p.restore();
    }
    break;
  case Symbol::Usb:
    p.drawLine(12, 20, 12, 3);
    p.drawLine(12, 3, 9, 6);
    p.drawLine(12, 3, 15, 6);
    p.drawLine(12, 15, 6, 11);
    p.drawLine(6, 11, 6, 7);
    p.drawLine(12, 12, 18, 8);
    p.drawEllipse(QPointF(12, 21), 1.5, 1.5);
    p.drawEllipse(QPointF(6, 6), 1, 1);
    p.drawRect(QRectF(17, 5, 2, 2));
    break;
  case Symbol::Power: {
    QPolygonF bolt{QPointF(13, 2), QPointF(6, 13), QPointF(11, 13),
                   QPointF(9, 22), QPointF(19, 9), QPointF(13, 9)};
    p.setBrush(color);
    p.drawPolygon(bolt);
    break;
  }
  case Symbol::Messages:
    p.drawRect(QRectF(3, 6, 18, 12));
    p.drawLine(3, 6, 12, 13);
    p.drawLine(21, 6, 12, 13);
    break;
  case Symbol::Charts: {
    QPolygonF wave{QPointF(2, 13), QPointF(6, 13),  QPointF(8, 6),  QPointF(11, 20),
                   QPointF(14, 3), QPointF(17, 15), QPointF(22, 11)};
    p.drawPolyline(wave);
    break;
  }
  case Symbol::Both:
    p.drawRect(QRectF(3, 5, 18, 14));
    p.drawLine(12, 5, 12, 19);
    p.drawLine(3, 9, 21, 9);
    break;
  }
  return QIcon(pixmap);
}

Lamp::Lamp(QString text, QWidget* parent) : QWidget(parent), text_(std::move(text)) {
  setAccessibleName(text_);
  setAccessibleDescription(QStringLiteral("Unavailable"));
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void Lamp::setLit(bool lit) {
  if (lit_ == lit)
    return;
  lit_ = lit;
  setAccessibleDescription(lit ? QStringLiteral("Present") : QStringLiteral("Unavailable"));
  update();
}

QSize Lamp::sizeHint() const { return {155, 19}; }

void Lamp::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QColor("#ededed"));
  p.drawText(QRect(0, 0, width() - 22, height()), Qt::AlignVCenter | Qt::AlignRight, text_);
  p.setPen(Qt::NoPen);
  p.setBrush(lit_ ? QColor("#58dd0b") : QColor("#ef302c"));
  p.drawRect(width() - 17, (height() - 8) / 2, 16, 8);
}

Battery::Battery(QWidget* parent) : QWidget(parent) {
  setAccessibleName(QStringLiteral("Simulated battery voltage"));
}

void Battery::setVoltage(double voltage, bool connected) {
  voltage_ = voltage;
  connected_ = connected;
  update();
}

QSize Battery::sizeHint() const { return {74, 40}; }

void Battery::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const QRectF body(3, 8, width() - 9, height() - 12);
  p.setPen(QPen(QColor("#a0a0a0"), 2));
  p.setBrush(QColor("#303030"));
  p.drawRect(body);
  p.fillRect(QRectF(width() * 0.35, 3, width() * 0.25, 4), QColor("#a0a0a0"));
  if (connected_) {
    const double fraction = std::clamp((voltage_ - 7.0) / 6.0, 0.0, 1.0);
    p.fillRect(QRectF(body.left() + 3, body.bottom() - 3 - fraction * (body.height() - 6),
                      body.width() - 6, fraction * (body.height() - 6)),
               QColor("#64c71c"));
  }
}

PovIndicator::PovIndicator(QWidget* parent) : QWidget(parent) {
  setFixedSize(42, 48);
  setAccessibleName(QStringLiteral("POV unavailable"));
}

void PovIndicator::setAngle(std::optional<int> angle) {
  angle_ = angle;
  const auto text = !angle       ? QStringLiteral("POV unavailable")
                    : *angle < 0 ? QStringLiteral("POV centered")
                                 : QStringLiteral("POV %1 degrees").arg(*angle);
  setAccessibleName(text);
  setToolTip(text);
  update();
}

void PovIndicator::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  for (int i = 0; i < 8; ++i) {
    painter.setBrush(angle_ && *angle_ == i * 45 ? QColor("#56bb1d") : QColor("#303030"));
    const double angle = i * 3.14159265358979323846 / 4;
    painter.drawEllipse(QPointF(21 + std::sin(angle) * 14, 23 - std::cos(angle) * 14), 4, 4);
  }
  painter.setBrush(angle_ && *angle_ == -1 ? QColor("#56bb1d") : QColor("#303030"));
  painter.drawEllipse(QPointF(21, 23), 4, 4);
}

RumbleBar::RumbleBar(QWidget* parent) : QWidget(parent), timer_(this) {
  setFixedSize(40, 11);
  timer_.setInterval(50);
  connect(&timer_, &QTimer::timeout, this, [this] {
    if (!held_ || !isVisible() || !isEnabled() || pulseClock_.restart() > 200) {
      cancel();
      return;
    }
    emit strengthChanged();
  });
}

void RumbleBar::cancel() {
  timer_.stop();
  if (!held_ && strength_ == 0)
    return;
  held_ = false;
  strength_ = 0;
  update();
  emit strengthChanged();
}

bool RumbleBar::event(QEvent* event) {
  if (event->type() == QEvent::Hide || event->type() == QEvent::UngrabMouse ||
      event->type() == QEvent::WindowDeactivate ||
      (event->type() == QEvent::EnabledChange && !isEnabled()))
    cancel();
  return QWidget::event(event);
}

void RumbleBar::setPosition(double x) {
  strength_ = static_cast<quint16>(std::clamp(x / std::max(1, width() - 1), 0.0, 1.0) * 65535);
  update();
  emit strengthChanged();
}

void RumbleBar::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || !isEnabled())
    return;
  held_ = true;
  pulseClock_.start();
  timer_.start();
  setPosition(event->position().x());
  event->accept();
}

void RumbleBar::mouseMoveEvent(QMouseEvent* event) {
  if (held_)
    setPosition(event->position().x());
}

void RumbleBar::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton)
    cancel();
}

void RumbleBar::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.fillRect(rect(), QColor("#303030"));
  painter.fillRect(QRectF(0, 0, width() * strength_ / 65535.0, height()), QColor("#56bb1d"));
}

TelemetryChart::TelemetryChart(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("telemetryChart"));
  setMinimumSize(145, 115);
  setSource(Source::Simulation);
}

void TelemetryChart::setSource(Source source) {
  source_ = source;
  setAccessibleName(source == Source::Network
                        ? QStringLiteral("Received UDP round trip time and battery history")
                        : QStringLiteral("Simulated trip time and battery history"));
  clear();
}

void TelemetryChart::appendSample(double seconds, double trip, double voltage, bool startsSegment) {
  if (!std::isfinite(seconds) || !std::isfinite(trip) || !std::isfinite(voltage) ||
      seconds < 0 || trip < 0 || voltage < 0 ||
      (!samples_.isEmpty() && seconds < samples_.last().seconds))
    return;
  samples_.append({seconds, trip, voltage, startsSegment});
  if (samples_.size() > 3000)
    samples_.remove(0, 1);
  endSeconds_ = std::max(endSeconds_, seconds);
  setToolTip(QStringLiteral("%1: %2 ms · Battery: %3 V")
                 .arg(source_ == Source::Network ? QStringLiteral("Received RTT")
                                                 : QStringLiteral("Simulated trip"))
                 .arg(trip, 0, 'f', 1).arg(voltage, 0, 'f', 2));
  update();
}

void TelemetryChart::advanceTime(double seconds) {
  if (std::isfinite(seconds) && seconds > endSeconds_)
    endSeconds_ = seconds;
  update();
}

void TelemetryChart::clear() {
  samples_.clear();
  endSeconds_ = 0;
  setToolTip(source_ == Source::Network ? QStringLiteral("No received UDP samples.")
                                       : QStringLiteral("No simulated samples."));
  update();
}
void TelemetryChart::setWindowSeconds(int seconds) {
  if (seconds != 12 && seconds != 60 && seconds != 300)
    return;
  windowSeconds_ = seconds;
  update();
}

void TelemetryChart::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const int chartHeight = (height() - 38) / 2;
  const QRectF top(25, 9, width() - 35, chartHeight);
  const QRectF bottom(25, 22 + chartHeight, width() - 35, chartHeight);
  p.setFont(QFont(QStringLiteral("Sans Serif"), 7));
  for (const auto& rect : {top, bottom}) {
    p.fillRect(rect, QColor("#2a2a2a"));
    p.setPen(QColor("#555555"));
    p.drawRect(rect);
    p.setPen(QPen(QColor("#454545"), 1, Qt::DotLine));
    p.drawLine(QPointF(rect.left(), rect.center().y()), QPointF(rect.right(), rect.center().y()));
  }
  p.setPen(QColor("#64e649"));
  p.drawText(0, 14, QStringLiteral("ms"));
  p.setPen(QColor("#f4dc1b"));
  p.drawText(5, static_cast<int>(bottom.top()) + 10, QStringLiteral("V"));
  p.setPen(QColor("#b9b9b9"));
  p.drawText(QRectF(0, height() - 15, width(), 15), Qt::AlignCenter,
             QStringLiteral("%1 s · %2").arg(windowSeconds_)
                 .arg(source_ == Source::Network ? QStringLiteral("received UDP")
                                                 : QStringLiteral("simulated")));
  if (samples_.isEmpty()) {
    p.drawText(top.united(bottom), Qt::AlignCenter,
               source_ == Source::Network ? QStringLiteral("No received data")
                                          : QStringLiteral("No demo data"));
    return;
  }
  const double end = endSeconds_;
  double tripScale = 20;
  for (const auto& sample : samples_)
    if (end - sample.seconds <= windowSeconds_)
      tripScale = std::max(tripScale, std::ceil(sample.trip / 20.0) * 20.0);
  p.setPen(QColor("#64e649"));
  p.drawText(0, 26, QString::number(tripScale, 'f', 0));
  const auto plot = [&](const QRectF& rect, bool battery) {
    QPainterPath path;
    bool first = true;
    for (const auto& sample : samples_) {
      const double age = end - sample.seconds;
      if (age > windowSeconds_)
        continue;
      const double value = battery ? sample.voltage / 16.0 : sample.trip / tripScale;
      const QPointF point(rect.right() - age / windowSeconds_ * rect.width(),
                          rect.bottom() - std::clamp(value, 0.0, 1.0) * rect.height());
      if (first || sample.startsSegment) {
        path.moveTo(point);
        first = false;
      } else {
        path.lineTo(point);
      }
    }
    p.setPen(QPen(battery ? QColor("#f4dc1b") : QColor("#64e649"), 1.5));
    p.drawPath(path);
  };
  plot(top, false);
  plot(bottom, true);
}

} // namespace ds
