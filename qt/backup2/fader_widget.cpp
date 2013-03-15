#include <QtGui>
#include <QDebug>
#include <windows.h>


#include "qt/backup2/fader_widget.h"

FaderWidget::FaderWidget(QWidget *parent)
    : QWidget(parent)
{
    if (parent)
        startColor = parent->palette().window().color();
    else
        startColor = Qt::white;

    currentAlpha = 0;
    duration = 500;

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));

    setAttribute(Qt::WA_DeleteOnClose);
    resize(parent->size());
}

void FaderWidget::start() {
  mutex_.lock();
  currentAlpha = 255;
  reverse_direction_ = false;
  timer->start(5);
  show();
}

void FaderWidget::reverse() {
  mutex_.lock();
  currentAlpha = 0;
  reverse_direction_ = true;
  timer->start(5);
  show();
}

void FaderWidget::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter(this);
    if (currentAlpha > 255 || currentAlpha < 0) {
      return;
    }
    QColor semiTransparentColor = startColor;
    semiTransparentColor.setAlpha(currentAlpha);
    painter.fillRect(rect(), semiTransparentColor);

    if (reverse_direction_) {
      currentAlpha += 255 * timer->interval() / duration;
      if (currentAlpha >= 255) {
          timer->stop();
          mutex_.unlock();
          close();
      }
    } else {
      currentAlpha -= 255 * timer->interval() / duration;
      if (currentAlpha <= 0) {
          timer->stop();
          mutex_.unlock();
          close();
      }
    }
}

void FaderWidget::close() {
  while (true) {
    qDebug() << "Grab lock";
    mutex_.lock();
    qDebug() << "Check";
    if ((currentAlpha <= 0 && !reverse_direction_) ||
        (currentAlpha >= 255 && reverse_direction_)) {
      qDebug() << "Got it!";
      mutex_.unlock();
      break;
    }
    qDebug() << "Unlocking";
    mutex_.unlock();
    Sleep(100);
  }
  QWidget::close();
}
