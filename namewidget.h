#ifndef NAMEWIDGET_H
#define NAMEWIDGET_H

#include <QtGui>

class NameWidget : public QWidget
{
    Q_OBJECT

    QTimer *timer_scroll;
    QString text;
    int step;
    int text_descent;
    int text_height;
    int text_wid;

protected:
    void paintEvent(QPaintEvent *event);
    virtual QSize sizeHint() const;

private slots:
    void timerSlot();

public:
    explicit NameWidget(QWidget *parent = 0);

    void setText(QString text);
    void activateTimers(bool activate) {
        if( timer_scroll != NULL ) {
            activate ? timer_scroll->start() : timer_scroll->stop();
        }
    }
};

#endif // NAMEWIDGET_H
