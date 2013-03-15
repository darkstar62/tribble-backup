#ifndef BACKUP2_QT_BACKUP2_FADER_WIDGET_H_
#define BACKUP2_QT_BACKUP2_FADER_WIDGET_H_

#include <QColor>
#include <QMutex>
#include <QTimer>
#include <QWidget>

class FaderWidget : public QWidget {
  Q_OBJECT
  Q_PROPERTY(QColor fadeColor READ fadeColor \
             WRITE setFadeColor)
  Q_PROPERTY(int fadeDuration READ fadeDuration \
             WRITE setFadeDuration)
 public:
  FaderWidget(QWidget *parent);

  QColor fadeColor() const { return startColor; }
  void setFadeColor(const QColor &newColor) { startColor = newColor; }
  int fadeDuration() const { return duration; }
  void setFadeDuration(int milliseconds) { duration = milliseconds; }
  void start();
  void reverse();

  virtual void close();

 protected:
  void paintEvent(QPaintEvent *event);

 private:
  QTimer *timer;
  int currentAlpha;
  QColor startColor;
  int duration;
  bool reverse_direction_;
  QMutex mutex_;
};

#endif // BACKUP2_QT_BACKUP2_FADER_WIDGET_H_
