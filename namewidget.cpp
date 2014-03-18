#include "main.h"
#include "namewidget.h"

NameWidget::NameWidget(QWidget *parent) :
    QWidget(parent), timer_scroll(NULL), step(0), text_descent(0), text_height(0), text_wid(0)
{
    timer_scroll = new QTimer(this);
    connect(timer_scroll, SIGNAL(timeout()), this, SLOT(timerSlot()));
    timer_scroll->start(20);

    //this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    this->setText("");
}

void NameWidget::timerSlot() {
    //qDebug("NameWidget::timerSlot");
    //qApp->beep();
    if( !myApp->isActive() ) {
        return;
    }
    this->step += 2;
    this->update();
}

void NameWidget::paintEvent(QPaintEvent *event) {
    //qDebug("namewidget paint");
    //qApp->beep();
    if( !myApp->isActive() ) {
        return;
    }
    int this_h = this->height();
    /*qDebug("text_h: %d", text_h);
    qDebug("this_h: %d", this_h);*/
    int x_pos = this->width() - step;
    if( x_pos < - text_wid ) {
        // wraparound
        qDebug("wraparound: %d, %d", x_pos, text_wid);
        step = 0;
    }
    QPainter painter(this);
    // n.b., text color is set in MainWindow constructor
    painter.drawText(x_pos, (this_h + text_height)/2 - text_descent, text);
    //qDebug(">>> %s", text.toStdString().c_str());
    //painter.drawText(x_pos, this_h - text_descent + text_height/2, text);
}

void NameWidget::setText(QString text) {
    this->text = text;
    QFontMetrics fontMetrics(this->font());
    this->text_descent = fontMetrics.descent();
    this->text_height = fontMetrics.height();
    this->text_wid = fontMetrics.width(text);
    /*qDebug("set text to: %s", text.toStdString().c_str());
    qDebug("text_descent: %d", text_descent);
    qDebug("text_height: %d", text_height);
    qDebug("text_wid: %d", text_wid);*/
}

QSize NameWidget::sizeHint() const {
    QSize size;
    size.setWidth(0);
    QFontMetrics fontMetrics(this->font());
    size.setHeight(fontMetrics.height());
    return size;
}
