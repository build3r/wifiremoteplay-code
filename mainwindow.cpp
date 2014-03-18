#include "main.h"
#include "mainwindow.h"

#include <QWebFrame>
#include <QWebHistory>
#include <QXmlStreamReader>

#include <algorithm>
using std::min;

#include <QXmlStreamReader>

const QString player_key_c = "player";
const QString ip_address_key_c = "ip_address";
const QString mpc_port_key_c = "port";
const QString vlc_port_key_c = "vlcport";
const int default_mpc_port_c = 13579;
const int default_vlc_port_c = 8080;

void WebViewEventFilter::setWebView(QWebView *webView) {
    qDebug("setWebView");
    this->webView = webView;
    this->webView->installEventFilter(this);
}

bool WebViewEventFilter::eventFilter(QObject *obj, QEvent *event) {
    switch( event->type() ) {
        case QEvent::MouseButtonPress:
            {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                if( mouseEvent->button() == Qt::LeftButton ) {
                    //filterMouseMove = true;
                    // disallow selection - but we need to allow click and drag for the scroll bars!
                    if( webView != NULL ) {
                        QRect vertRect = webView->page()->mainFrame()->scrollBarGeometry(Qt::Vertical);
                        QRect horizRect = webView->page()->mainFrame()->scrollBarGeometry(Qt::Horizontal);
                        int scrollbar_x = vertRect.left();
                        int scrollbar_y = horizRect.top();
                        if( ( vertRect.isEmpty() || mouseEvent->x() <= scrollbar_x ) && ( horizRect.isEmpty() || mouseEvent->y() <= scrollbar_y ) ) {
                            // disallow mouse dragging
                            filterMouseMove = true;
                            orig_mouse_x = saved_mouse_x = mouseEvent->globalX();
                            orig_mouse_y = saved_mouse_y = mouseEvent->globalY();
                        }
                    }
                }
                break;
            }
        case QEvent::MouseButtonRelease:
            {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                if( mouseEvent->button() == Qt::LeftButton ) {
                    filterMouseMove = false;
                    // if we've clicked and dragged, we don't want to allow clicking links
                    int mouse_x = mouseEvent->globalX();
                    int mouse_y = mouseEvent->globalY();
                    int diff_x = mouse_x - orig_mouse_x;
                    int diff_y = mouse_y - orig_mouse_y;
                    int dist2 = diff_x*diff_x + diff_y*diff_y;
                    const int tolerance = 16;
                    qDebug("drag %d, %d : dist2: %d", diff_x, diff_y, dist2);
                    if( dist2 > tolerance*tolerance ) {
                        // need to allow some tolerance, otherwise hard to click on a touchscreen device!
                        return true;
                    }
                }
               break;
            }
        case QEvent::MouseMove:
            {
                /*if( filterMouseMove && webView != NULL ) {
                    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                    int scrollbar_x = webView->page()->mainFrame()->scrollBarGeometry(Qt::Vertical).left();
                    int scrollbar_y = webView->page()->mainFrame()->scrollBarGeometry(Qt::Horizontal).top();
                    if( mouseEvent->x() <= scrollbar_x && mouseEvent->y() <= scrollbar_y ) {
                        // filter
                        return true;
                    }
                }*/
                if( filterMouseMove ) {
                    if( webView != NULL ) {
                        // support for swype scrolling
                        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                        int new_mouse_x = mouseEvent->globalX();
                        int new_mouse_y = mouseEvent->globalY();
                        webView->page()->mainFrame()->scroll(saved_mouse_x - new_mouse_x, saved_mouse_y - new_mouse_y);
                        saved_mouse_x = new_mouse_x;
                        saved_mouse_y = new_mouse_y;
                    }
                    return true;
                }
            }
            break;
        default:
            break;
    }
    return false;
}

/*bool MyEventFilter::eventFilter(QObject *obj, QEvent *event) {
    return true;
}*/

void TimeProgressBar::registerClick(int m_x) {
    float ratio = ((float)m_x) / (float)this->width();
    int slider_time = (int)(ratio * this->maximum());
    qDebug("slider moved to %d ( m_x %d , ratio %f )", slider_time, m_x, ratio);
    mainWindow->moveToTime(slider_time);
}

void TimeProgressBar::mousePressEvent(QMouseEvent *event) {
    this->mouse_down = true;
    int m_x = event->x();
    this->registerClick(m_x);
}

void TimeProgressBar::mouseReleaseEvent(QMouseEvent *event) {
    this->mouse_down = false;
}

void TimeProgressBar::mouseMoveEvent(QMouseEvent *event) {
    // allows dragging
    if( mouse_down ) {
        int m_x = event->x();
        this->registerClick(m_x);
    }
}

void SettingsWindow::closeEvent(QCloseEvent *event) {
    qDebug("SettingsWindow received close event");
    event->ignore();
    this->mainWindow->clickedSettingsCancel();
}

void DVDWindow::closeEvent(QCloseEvent *event) {
    qDebug("DVDWindow received close event");
    event->ignore();
    this->mainWindow->clickedDVDClose();
}

void BrowserWindow::closeEvent(QCloseEvent *event) {
    qDebug("BrowserWindow received close event");
    event->ignore();
    this->mainWindow->clickedBrowserClose();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      timer_request_status(NULL), timer_refresh_time(NULL),
      player(PLAYER_MPC),
      socket(NULL), /*reply(NULL),*/ command(-1), waiting_for_status(false), is_paused(true), have_time(false), have_saved_last_time(false), is_muted(false), saved_volume(0),
      /*nameLabel(NULL),*/ nameWidget(NULL), timeLabel(NULL), lengthLabel(NULL), /*slider(NULL), slider_pressed(false),*/ progress(NULL), settingsWindow(NULL), ipAddressLineEdit(NULL), portLineEdit(NULL),
      playerWindow(NULL), dvdWindow(NULL), webViewEventFilter(NULL), browserWindow(NULL), webView(NULL), done_html_modification(false),
      mpc_port(0), vlc_port(0)
{
    //ip_address = "192.168.1.2"; // laptop
    //ip_address = "192.168.1.1"; // desktop

    // font hackery - defaults for each platform don't seem quite right for our needs...
#if defined(Q_OS_SYMBIAN) || defined(Q_WS_MAEMO_5) || defined(Q_WS_SIMULATOR)
    QFont newFont = font();
    qDebug("current font size: %d", newFont.pointSize());
    newFont.setPointSize(newFont.pointSize() + 2);
    this->setFont(newFont);
#elif defined(Q_OS_ANDROID)
    // make work better on Android phones with crappy resolution
    // these settings determined by experimenting with emulator...
    int min_size = min(QApplication::desktop()->width(), QApplication::desktop()->height());
    QFont newFont = font();
    qDebug("current font size: %d", newFont.pointSize());
    qDebug("min_size: %d", min_size);
    if( min_size < 320 ) {
        newFont.setPointSize(newFont.pointSize() - 6);
    }
    else if( min_size < 480 ) {
        newFont.setPointSize(newFont.pointSize() - 4);
    }
    this->setFont(newFont);
#endif

    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, Qt::black);
    palette.setColor(QPalette::WindowText, Qt::green);
    palette.setColor(QPalette::Button, Qt::black);
    // changing button text colour doesn't seem to work on Symbian anyway?!
    // also none of this seems to have any effect on Android since necessitas alpha 4
#if defined(Q_OS_SYMBIAN) || defined(Q_WS_MAEMO_5) || defined(Q_WS_SIMULATOR)
    palette.setColor(QPalette::ButtonText, Qt::white);
#else
    palette.setColor(QPalette::ButtonText, Qt::green);
#endif
    this->setPalette(palette);

    settings = new QSettings(tr("Mark Harman"), tr("Wifi Remote Play"));
    bool ok = false;

    player = static_cast<Player>(settings->value(player_key_c, PLAYER_MPC).toInt(&ok));
    if( !ok ) {
        qDebug("settings player not ok, set to default");
        player = PLAYER_MPC;
    }

    ip_address = settings->value(ip_address_key_c).toString();

    mpc_port = settings->value(mpc_port_key_c, default_mpc_port_c).toInt(&ok);
    if( !ok ) {
        qDebug("settings mpc port not ok, set to default");
        mpc_port = default_mpc_port_c;
    }

    vlc_port = settings->value(vlc_port_key_c, default_vlc_port_c).toInt(&ok);
    if( !ok ) {
        qDebug("settings vlc port not ok, set to default");
        vlc_port = default_vlc_port_c;
    }

    qDebug("Player: %d", player);
    qDebug("IP Address: %s", ip_address.toStdString().c_str());
    qDebug("MPC Port: %d", mpc_port);
    qDebug("VLC Port: %d", vlc_port);

    this->createUI();

    connect(&qnam, SIGNAL(finished(QNetworkReply *)), this, SLOT(qnamFinished(QNetworkReply *)));

    timer_request_status = new QTimer(this);
    connect(timer_request_status, SIGNAL(timeout()), this, SLOT(requestStatus()));
    timer_request_status->start(5000);

    timer_refresh_time = new QTimer(this);
    connect(timer_refresh_time, SIGNAL(timeout()), this, SLOT(refreshTimeLabel()));
    /* Timer doesn't seem to be as accurate on Android, and sometimes is at eratic, often shorter, intervals than requested.
     * For the timer_refresh_time, this means we need an interval much shorter than a second, otherwise the timer will seem
     * to update in a jerky manner.
     */
#if defined(Q_OS_ANDROID)
    timer_refresh_time->start(100);
#else
    timer_refresh_time->start(1000);
#endif

    status_last_check_timer.invalidate(); // just to be sure it starts off invalid

    webViewEventFilter = new WebViewEventFilter(this);
}

MainWindow::~MainWindow()
{
    qDebug("MainWindow destructor");
    myApp->setMainWindow(NULL);

    if( socket != NULL ) {
        delete socket;
    }
    /*if( reply != NULL ) {
        delete reply;
    }*/
    delete settings;
    qDebug("MainWindow destructor done");
}

void MainWindow::closeEvent(QCloseEvent *event) {
    qDebug("MainWindow::closeEvent()");
    // use a close event to quit, otherwise application still running when window closed (at least when testing on Windows)?!
    qApp->quit();
}

void MainWindow::createUI() {
    // create main GUI

    if( this->centralWidget() != NULL ) {
        delete this->centralWidget();
    }

    QWidget *centralWidget = new QWidget(this);
    centralWidget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    this->setCentralWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout();
    centralWidget->setLayout(layout);

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        nameWidget = new NameWidget(this);
        h_layout->addWidget(nameWidget);
    }

    /*{
        //this->setStyleSheet("QSlider::groove:horizontal { border: 1px solid #999999; height: 1px; background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #B1B1B1, stop:1 #c4c4c4); margin: 2px 0; }");
        //this->setStyleSheet("QSlider::handle:horizontal { background: green; border: 1px solid #5c5c5c; width: 1px; margin: -2px 0; border-radius: 1px; }");
        slider = new QSlider(Qt::Horizontal);
        slider->setStyleSheet("QSlider::groove:horizontal { border: 1px solid #999999; height: 8px; background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #B1B1B1, stop:1 #c4c4c4); margin: 2px 0; }");
        //slider->setStyleSheet("QSlider::handle:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #b4b4b4, stop:1 #8f8f8f); border: 1px solid #5c5c5c; width: 18px; margin: -2px 0; border-radius: 3px; }");
        //slider->setStyleSheet("QSlider::handle:horizontal { background: green; border: 1px solid #5c5c5c; width: 16px; margin: -2px 0; border-radius: 1px; }");
        slider->setStyleSheet("QSlider::handle:horizontal { background: red; width: 1px; height 1px; }");
        //slider->setStyleSheet("QSlider::handle:horizontal { width: 36px; ");
        slider->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
        slider->setMinimum(0);
        slider->setMaximum(0);
        slider->setValue(0);
        layout->addWidget(slider);
        connect(slider, SIGNAL(sliderPressed()), this, SLOT(sliderPressed()));
        connect(slider, SIGNAL(sliderReleased()), this, SLOT(sliderReleased()));
    }*/
    {
        //progress = new QProgressBar();
        progress = new TimeProgressBar(this);
        /*progress->setStyleSheet(
                    "QProgressBar:horizontal { "
                    "border: 2px solid grey; "
                    "border-radius: 5px; "
                    "text-align: center; "
                    "background: white; "
                    "color: black; "
                    "} "
                    "QProgressBar::chunk:horizontal { "
                    "background: green; "
                    "width: 20px; "
                    "}"
                    );*/
        //progress->setPalette(this->palette());
        progress->setMinimum(0);
        progress->setMaximum(0);
        progress->setValue(0);
        layout->addWidget(progress);
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        timeLabel = new QLabel("");
        h_layout->addWidget(timeLabel);

        lengthLabel = new QLabel("");
        h_layout->addWidget(lengthLabel);

    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *prevButton = new QPushButton("<<");
        h_layout->addWidget(prevButton);
        connect(prevButton, SIGNAL(clicked()), this, SLOT(clickedPrev()));
        this->initButton(prevButton);

        QPushButton *bwdButton = new QPushButton("<");
        h_layout->addWidget(bwdButton);
        connect(bwdButton, SIGNAL(clicked()), this, SLOT(clickedBwd()));
        this->initButton(bwdButton);

        QPushButton *playButton = new QPushButton(tr("Play/Pause"));
        h_layout->addWidget(playButton);
        connect(playButton, SIGNAL(clicked()), this, SLOT(clickedPlay()));
        this->initButton(playButton);

        QPushButton *fwdButton = new QPushButton(">");
        h_layout->addWidget(fwdButton);
        connect(fwdButton, SIGNAL(clicked()), this, SLOT(clickedFwd()));
        this->initButton(fwdButton);

        QPushButton *nextButton = new QPushButton(">>");
        h_layout->addWidget(nextButton);
        connect(nextButton, SIGNAL(clicked()), this, SLOT(clickedNext()));
        this->initButton(nextButton);
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *volumeMinusButton = new QPushButton("-");
        h_layout->addWidget(volumeMinusButton);
        connect(volumeMinusButton, SIGNAL(clicked()), this, SLOT(clickedVolumeMinus()));
        this->initButton(volumeMinusButton);

        QPushButton *volumePlusButton = new QPushButton("+");
        h_layout->addWidget(volumePlusButton);
        connect(volumePlusButton, SIGNAL(clicked()), this, SLOT(clickedVolumePlus()));
        this->initButton(volumePlusButton);

        QPushButton *volumeMuteButton = new QPushButton("mute");
        h_layout->addWidget(volumeMuteButton);
        connect(volumeMuteButton, SIGNAL(clicked()), this, SLOT(clickedVolumeMute()));
        this->initButton(volumeMuteButton);

        if( player == PLAYER_MPC ) {
            QPushButton *dvdButton = new QPushButton("DVD");
            h_layout->addWidget(dvdButton);
            connect(dvdButton, SIGNAL(clicked()), this, SLOT(openDVDWindow()));
            this->initButton(dvdButton);
        }
    }

    // create menu

    delete this->menuBar();
    QMenuBar *menuBar = this->menuBar();

    QWidget *menuOptions = NULL;
#if defined(Q_OS_SYMBIAN) || defined(Q_WS_MAEMO_5) || defined(Q_WS_SIMULATOR)
    // on Symbian/Maemo/Meego, we can add direct to the main menubar
    menuOptions = menuBar;
#else
    menuOptions = menuBar->addMenu("Wifi Remote Play [Options]");
#endif

    QAction *settingsAction = new QAction(tr("Settings"), this);
    connect(settingsAction, SIGNAL(triggered()), this, SLOT(openSettingsWindow()));
    menuOptions->addAction(settingsAction);

    QAction *fullscreenAction = new QAction(tr("Toggle full screen"), this);
    connect(fullscreenAction, SIGNAL(triggered()), this, SLOT(toggleFullscreen()));
    menuOptions->addAction(fullscreenAction);

    if( player == PLAYER_MPC ) {
        QAction *browserAction = new QAction(tr("Open file browser"), this);
        connect(browserAction, SIGNAL(triggered()), this, SLOT(openBrowserWindow()));
        menuOptions->addAction(browserAction);
    }

    QAction *playerAction = new QAction(tr("Choose player"), this);
    connect(playerAction, SIGNAL(triggered()), this, SLOT(openPlayerWindow()));
    menuOptions->addAction(playerAction);

    QAction *helpAction = new QAction(tr("Online help"), this);
    connect(helpAction, SIGNAL(triggered()), this, SLOT(help()));
    menuOptions->addAction(helpAction);

    QAction *donateAction = new QAction(tr("Make a donation"), this);
    connect(donateAction, SIGNAL(triggered()), this, SLOT(donate()));
    menuOptions->addAction(donateAction);

    QAction *webifaceAction = new QAction(tr("Web interface"), this);
    connect(webifaceAction, SIGNAL(triggered()), this, SLOT(webiface()));
    menuOptions->addAction(webifaceAction);

    QAction *aboutAction = new QAction(tr("About"), this);
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));
    menuOptions->addAction(aboutAction);

}

void MainWindow::setOrientation(ScreenOrientation orientation)
{
#if defined(Q_OS_SYMBIAN)
    // If the version of Qt on the device is < 4.7.2, that attribute won't work
    if (orientation != ScreenOrientationAuto) {
        const QStringList v = QString::fromAscii(qVersion()).split(QLatin1Char('.'));
        if (v.count() == 3 && (v.at(0).toInt() << 16 | v.at(1).toInt() << 8 | v.at(2).toInt()) < 0x040702) {
            qWarning("Screen orientation locking only supported with Qt 4.7.2 and above");
            return;
        }
    }
#endif // Q_OS_SYMBIAN

    Qt::WidgetAttribute attribute;
    switch (orientation) {
#if QT_VERSION < 0x040702
    // Qt < 4.7.2 does not yet have the Qt::WA_*Orientation attributes
    case ScreenOrientationLockPortrait:
        attribute = static_cast<Qt::WidgetAttribute>(128);
        break;
    case ScreenOrientationLockLandscape:
        attribute = static_cast<Qt::WidgetAttribute>(129);
        break;
    default:
    case ScreenOrientationAuto:
        attribute = static_cast<Qt::WidgetAttribute>(130);
        break;
#else // QT_VERSION < 0x040702
    case ScreenOrientationLockPortrait:
        attribute = Qt::WA_LockPortraitOrientation;
        break;
    case ScreenOrientationLockLandscape:
        attribute = Qt::WA_LockLandscapeOrientation;
        break;
    default:
    case ScreenOrientationAuto:
        attribute = Qt::WA_AutoOrientation;
        break;
#endif // QT_VERSION < 0x040702
    };
    setAttribute(attribute, true);
}

void MainWindow::showExpanded()
{
#if defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR)
    //showFullScreen();
    showMaximized();
#elif defined(Q_WS_MAEMO_5)
    showMaximized();
#else
    show();
#endif
}

void MainWindow::initButton(QPushButton *button) {
    button->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    //button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    /* Minimum width hackery:
      * On Android, the style seems to enforce a minimum width such that the buttons are too wide (even with a tiny font), meaning
      * the UI can't be viewed properly if the resolution width is less than 600!
      * So we set an arbitrary small resolution - and if this is too small for the fonts, we'd still rather display the UI on-screen.
      * Even though this issue only shows with Android, might as well do it for all platforms anyway.
      */
    button->setMinimumWidth(8);
    /*int h = button->sizeHint().height();
    //qDebug("height = %d", h);
    h *= 1.8;
    //qDebug("scale height = %d", h);
    button->setMinimumHeight(h);*/
    /*h = button->sizeHint().height();
    qDebug("height = %d", h);*/
}

void MainWindow::clickedPrev() {
    qDebug("prev");
    if( have_time ) {
        have_time = false;
        this->refreshTimeLabel();
    }
    have_saved_last_time = false; // status checker may get confused, so need to reset
    if( player == PLAYER_MPC ) {
        //sendMPCCommand(919);
        sendMPCCommand(921);
    }
    else if( player == PLAYER_VLC ) {
        sendVLCCommand("pl_previous");
    }
}

void MainWindow::clickedNext() {
    qDebug("next");
    if( have_time ) {
        have_time = false;
        this->refreshTimeLabel();
    }
    have_saved_last_time = false; // status checker may get confused, so need to reset
    if( player == PLAYER_MPC ) {
        //sendMPCCommand(920);
        sendMPCCommand(922);
    }
    else if( player == PLAYER_VLC ) {
        sendVLCCommand("pl_next");
    }
}

void MainWindow::clickedBwd() {
    qDebug("bwd");
    if( have_time ) {
        time = time.addSecs(-5);
        this->refreshTimeLabel();
    }
    have_saved_last_time = false; // status checker may get confused, so need to reset
    if( player == PLAYER_MPC ) {
        sendMPCCommand(901);
    }
    else if( player == PLAYER_VLC ) {
        sendVLCCommand("seek&val=-5s");
    }
}

void MainWindow::clickedFwd() {
    qDebug("fwd");
    if( have_time ) {
        time = time.addSecs(5);
        this->refreshTimeLabel();
    }
    have_saved_last_time = false; // status checker may get confused, so need to reset
    if( player == PLAYER_MPC ) {
        sendMPCCommand(902);
    }
    else if( player == PLAYER_VLC ) {
        sendVLCCommand("seek&val=+5s");
    }
}

void MainWindow::clickedPlay() {
    qDebug("play");
    is_paused = !is_paused;
    have_saved_last_time = false; // status checker may get confused, so need to reset

    if( player == PLAYER_MPC ) {
        sendMPCCommand(889);
    }
    else if( player == PLAYER_VLC ) {
        sendVLCCommand("pl_pause");
    }
}

void MainWindow::clickedVolumeMinus() {
    qDebug("volume minus");
    if( player == PLAYER_MPC ) {
        sendMPCCommand(908);
    }
    else if( player == PLAYER_VLC ) {
        sendVLCCommand("volume&val=-20");
    }
}

void MainWindow::clickedVolumePlus() {
    qDebug("volume plus");
    if( player == PLAYER_MPC ) {
        sendMPCCommand(907);
    }
    else if( player == PLAYER_VLC ) {
        sendVLCCommand("volume&val=+20");
    }
}

void MainWindow::clickedVolumeMute() {
    qDebug("volume mute");
    if( player == PLAYER_MPC ) {
        sendMPCCommand(909);
    }
    else if( player == PLAYER_VLC ) {
        if( is_muted ) {
            sendVLCCommand("volume&val=" + QString::number(saved_volume));
        }
        else {
            sendVLCCommand("volume&val=0");
        }
        is_muted = !is_muted;
    }
}

/*void MainWindow::sliderPressed() {
    this->slider_pressed = true;
}

void MainWindow::sliderReleased() {
    this->slider_pressed = false;
    int slider_time = slider->value();
    qDebug("slider moved to %d", slider_time);
    QTime new_time(0, 0, 0, 0);
    new_time = new_time.addSecs(slider_time);
    if( have_time ) {
        time = new_time;
        this->refreshTimeLabel();
    }
    have_saved_last_time = false; // status checker may get confused, so need to reset
    if( player == PLAYER_MPC ) {
        sendMPCCommand(-1, true, "position", new_time.toString("hh:mm:ss"));
        //sendMPCCommand(-1, true, "percent", "50");
    }
    else if( player == PLAYER_VLC ) {
        sendVLCCommand("seek&val=" + QString::number(slider_time) + "s");
    }
}*/

void MainWindow::moveToTime(int slider_time) {
    QTime new_time(0, 0, 0, 0);
    new_time = new_time.addSecs(slider_time);
    if( have_time ) {
        time = new_time;
        this->refreshTimeLabel();
    }
    have_saved_last_time = false; // status checker may get confused, so need to reset
    if( player == PLAYER_MPC ) {
        sendMPCCommand(-1, true, "position", new_time.toString("hh:mm:ss"));
        //sendMPCCommand(-1, true, "percent", "50");
    }
    else if( player == PLAYER_VLC ) {
        sendVLCCommand("seek&val=" + QString::number(slider_time) + "s");
    }
}

void MainWindow::refreshTimeLabel() {
    //qDebug("MainWindow::refreshTimeLabel");
    //qApp->beep();
    if( !myApp->isActive() ) {
        return;
    }
    if( have_time ) {
        if( !is_paused ) {
            int elapsed = time_last_updated.restart();
            //qDebug("elapsed: %d", elapsed);
            time = time.addMSecs(elapsed);
        }
        this->timeLabel->setText( time.toString("hh:mm:ss") );
        /*if( !this->slider_pressed ) {
            this->slider->setValue(- time.secsTo(QTime(0, 0, 0, 0)));
        }*/
        this->progress->setValue(- time.secsTo(QTime(0, 0, 0, 0)));
    }
    else {
        this->timeLabel->setText("");
        /*if( !this->slider_pressed ) {
            this->slider->setValue(0);
        }*/
        this->progress->setValue(0);
    }
}

/*bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    qDebug("ping");
    //event->type() == QEvent::ApplicationActivate;
    if( event->type() == QEvent::ApplicationActivate ) {
        qDebug("application activated");
    }
    else if( event->type() == QEvent::ApplicationDeactivate ) {
        qDebug("application deactivated");
    }
    //return QMainWindow::eventFilter(obj, event);
    return false;
}*/

void MainWindow::sendMPCCommand(int command) {
    sendMPCCommand(command, false, "", "");
}

void MainWindow::sendMPCCommand(int command, bool has_extra, QString extra_name, QString extra_value) {
    this->command = command;
    this->has_extra = has_extra;
    this->extra_name = extra_name;
    this->extra_value = extra_value;
    if( !this->IPDefined() ) {
        // don't even bother trying
        qDebug("invalid ip");
        return;
    }
    if( socket != NULL ) {
        // cancel previous request
        delete socket;
    }
    socket = new QTcpSocket(this);
    QHostAddress host(ip_address);
    socket->connectToHost(host, mpc_port);
    connect(socket, SIGNAL(connected()), this, SLOT(connected()));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(networkError(QAbstractSocket::SocketError)));
}

void MainWindow::sendVLCCommand(QString command) {
    QUrl url("http://" + ip_address + ":" + QString::number(vlc_port) + "/requests/status.xml?command=" + command);
    QNetworkReply *reply = qnam.get(QNetworkRequest(url));
}

void MainWindow::connected() {
    if( socket == NULL ) {
        // shouldn't happen?!
    }
    qDebug("connected");
    QString command_string = "wm_command=" + QString::number(command);
    if( has_extra ) {
        command_string += "&" + extra_name + "=" + extra_value;
    }
    QString data = "POST /command.html HTTP/1.0\r\n";
    data += "HOST: localhost\r\n";
    data += "Content-Type: application/x-www-form-urlencoded\r\n";
    data += "Content-length: " + QString::number(command_string.length()) + "\r\n";
    data += "\r\n";
    data += command_string;
    data += "\r\n";
    data += "\r\n";
    //qDebug("sending:");
    //qDebug(data.toStdString().c_str());
    int written = socket->write(data.toStdString().c_str(), data.length());
    //qDebug("written: %d", written);
    /*QByteArray readBytes = socket->readAll();
    qDebug("read:\n%s", readBytes.data());*/
    /*delete socket;
    socket = NULL;*/
}

/*void MainWindow::networkError(QNetworkReply::NetworkError code) {
    qDebug("networkError (reply)");
    if( code != QNetworkReply::NoError && code != QNetworkReply::RemoteHostClosedError ) {
        // from http://labs.trolltech.com/blogs/2008/10/09/coding-tip-pretty-printing-enum-values/
        QString errorValue;
        QMetaObject meta = QNetworkReply::staticMetaObject;
        for(int i=0;i<meta.enumeratorCount();i++) {
            QMetaEnum m = meta.enumerator(i);
            if( m.name() == QLatin1String("NetworkError") ) {
                errorValue = QLatin1String(m.valueToKey(code));
                break;
            }
        }
        QMessageBox::critical(this, "Network Error", errorValue);
    }
}*/

void MainWindow::networkError(QAbstractSocket::SocketError socketError) {
    qDebug("networkError (socket)");
    if( socket != NULL ) {
        if( socketError == QAbstractSocket::RemoteHostClosedError || socketError == QAbstractSocket::NetworkError ) {
            // don't report this to user
            // n.b., QAbstractSocket::NetworkError is for "Bad file descriptor : 7" - unclear why this appears, but sometimes does, and just means annoying warning messages, even though the app still works
            qDebug("remote host closed");
        }
        else {
            QString message = socket->errorString() + " : " + QString::number((int)socketError);
            QMessageBox::critical(this, tr("Network Error"), message);
        }
        socket->deleteLater();
        socket = NULL;
    }
}

void MainWindow::requestStatus() {
    //qDebug("MainWindow::requestStatus");
    //qApp->beep();
    if( !this->IPDefined() ) {
        // don't even bother trying
        qDebug("invalid ip");
        return;
    }
    if( settingsWindow != NULL || playerWindow != NULL || dvdWindow != NULL || browserWindow != NULL ) {
        return;
    }
    if( !myApp->isActive() ) {
        return;
    }
    qDebug(ip_address.toStdString().c_str());
    if( waiting_for_status ) {
        // already sent a request
        qDebug("already sent a request");
        return;
    }
    waiting_for_status = true;
    /*if( reply != NULL ) {
        // delete previous request
        delete reply;
        reply = NULL;
    }*/
    qDebug("send status request");
    if( player == PLAYER_MPC ) {
        QUrl url("http://" + ip_address + ":" + QString::number(mpc_port) + "/status.html");
        QNetworkReply *reply = qnam.get(QNetworkRequest(url));
    }
    else if( player == PLAYER_VLC ) {
        QUrl url("http://" + ip_address + ":" + QString::number(vlc_port) + "/requests/status.xml");
        QNetworkReply *reply = qnam.get(QNetworkRequest(url));
    }
}

void MainWindow::parseStatusMetaInformation(QXmlStreamReader &reader) {
    // parses status from VLC in the <meta-information> ... </meta-information> [v1 API]
    qDebug("reading meta-information... [v1 API]");
    QString text_title, text_artist;
    while( !reader.atEnd() && !(reader.isEndElement() && reader.name() == "meta-information") ) {
        reader.readNext();
        if( reader.isStartElement() ) {
            qDebug("read: %s", reader.name().toString().toStdString().c_str());
            if( reader.name() == "title" ) {
                QString element = reader.readElementText();
                qDebug("title = %s", element.toStdString().c_str());
                // need to decode HTML "&" encoding etc
                QTextDocument document;
                document.setHtml(element);
                text_title = document.toPlainText();
            }
            else if( reader.name() == "artist" ) {
                QString element = reader.readElementText();
                qDebug("artist = %s", element.toStdString().c_str());
                // need to decode HTML "&" encoding etc
                QTextDocument document;
                document.setHtml(element);
                text_artist = document.toPlainText();
            }
        }
    }
    if( text_title.length() > 0 || text_artist.length() > 0 ) {
        QString text_name;
        if( text_artist.length() > 0 ) {
            text_name += text_artist;
        }
        if( text_title.length() > 0 ) {
            if( text_name.length() > 0 ) {
                text_name += " - ";
            }
            text_name += text_title;
        }
        this->nameWidget->setText(text_name);
    }
    qDebug("done reading meta-information!");
}

void MainWindow::parseStatusMetaCategory(QXmlStreamReader &reader) {
    // parses status from VLC in the <category name="meta"> ... </category> [v2 API]
    qDebug("reading category meta... [v2 API]");
    // It seems that if the file is tagged, "filename" isn't the actual filename!
    // So we prefer looking at title/artist instead, if available
    QString text_title, text_artist, text_filename;
    while( !reader.atEnd() && !(reader.isEndElement() && reader.name() == "category") ) {
        reader.readNext();
        if( reader.isStartElement() ) {
            qDebug("read: %s", reader.name().toString().toStdString().c_str());
            if( reader.name() == "info" ) {
                if( reader.attributes().value("name") == "title" ) {
                    QString element = reader.readElementText();
                    qDebug("title = %s", element.toStdString().c_str());
                    // need to decode HTML "&" encoding etc
                    QTextDocument document;
                    document.setHtml(element);
                    text_title = document.toPlainText();
                }
                else if( reader.attributes().value("name") == "artist" ) {
                    QString element = reader.readElementText();
                    qDebug("artist = %s", element.toStdString().c_str());
                    // need to decode HTML "&" encoding etc
                    QTextDocument document;
                    document.setHtml(element);
                    text_artist = document.toPlainText();
                }
                else if( reader.attributes().value("name") == "filename" ) {
                    QString element = reader.readElementText();
                    qDebug("filename = %s", element.toStdString().c_str());
                    // need to decode HTML "&" encoding etc
                    QTextDocument document;
                    document.setHtml(element);
                    text_filename = document.toPlainText();
                }
            }
        }
    }
    if( text_title.length() > 0 || text_artist.length() > 0 ) {
        QString text_name;
        if( text_artist.length() > 0 ) {
            text_name += text_artist;
        }
        if( text_title.length() > 0 ) {
            if( text_name.length() > 0 ) {
                text_name += " - ";
            }
            text_name += text_title;
        }
        this->nameWidget->setText(text_name);
    }
    else {
        this->nameWidget->setText(text_filename);
    }
    qDebug("done reading category meta!");
}

void MainWindow::parseStatusInformation(QXmlStreamReader &reader) {
    // parses status from VLC in the <information> ... </information>
    qDebug("reading information...");
    while( !reader.atEnd() && !(reader.isEndElement() && reader.name() == "information") ) {
        reader.readNext();
        if( reader.isStartElement() ) {
            qDebug("read: %s", reader.name().toString().toStdString().c_str());
            if( reader.name() == "meta-information" ) {
                parseStatusMetaInformation(reader);
            }
            else if( reader.name() == "category" ) {
                if( reader.attributes().value("name") == "meta" ) {
                    parseStatusMetaCategory(reader);
                }
                else {
                    // skip this category - read until the corresponding end element
                    reader.readElementText(QXmlStreamReader::SkipChildElements);
                }
            }
        }
    }
    qDebug("done reading information!");
}

void MainWindow::qnamFinished(QNetworkReply *reply) {
    qDebug("qnamFinished() received reply");

    //qDebug("waiting? %d", waiting_for_status?1:0);
    if( !waiting_for_status ) {
        reply->deleteLater();
        return;
    }
    waiting_for_status = false;

    if( status_last_check_timer.isValid() && status_last_check_timer.elapsed() < 2000 ) {
        // avoid checking too often - needed on Android where sometimes we may receive replies too quickly (which also causes the MPC "pause" checking to mess up...)
        //qDebug("wait longer: %d", status_last_check_timer.elapsed());
        reply->deleteLater();
        return;
    }
    status_last_check_timer.restart();

    if( player == PLAYER_MPC ) {
        QByteArray data = reply->readAll();
        // n.b., data may be empty, e.g. if MPC not running!
        qDebug("received: %s", data.data());
        QByteArray info_utf8 = data.mid(9); // ignore OnStatus(
        QString info = QString::fromUtf8(info_utf8);
        /*QUrl info_url(info);
        qDebug("encoded: %s", info_url.toEncoded().data());*/
        if( info.length() > 0 ) {
            info.chop(1); // chop off closing bracket
        }
        //qDebug("info: %s", info.toUtf8().data());

        //this->nameLabel->setText("");
        this->nameWidget->setText("");
        this->timeLabel->setText("");
        this->lengthLabel->setText("");
        /*if( !this->slider_pressed ) {
            this->slider->setValue(0);
        }*/
        this->progress->setValue(0);

        /*int index = info.lastIndexOf(" - Media");
        if( index == -1 ) {
            qDebug("can't find Media Player Classic string");
            return;
        }
        QString name = info.mid(1); // get rid of first quote
        name.truncate(index);
        this->nameWidget->setText(name);*/

        // find first ' character not as a \'
        bool found = false;
        int index = 0;
        char quote = '\'';
        // MPC 1.6.0.4014 and earlier uses ', 1.6.3.5818 and later uses " ...
        if( index < info.length() && info.at(index) == quote )
            index++; // ignore first '
        else if( index < info.length() && info.at(index) == '\"' ) {
            quote = '\"';
            index++; // ignore first '
        }
        int name_starts_at = index;
        while( !found ) {
            index = info.indexOf(quote, index);
            if( index == -1 ) {
                qDebug("can't find name string");
                return;
            }
            if( index > 0 && info.at(index-1) == '\\' ) {
                // not a real ', part of the name string, so carry on searching
                index++;
            }
            else {
                found = true;
            }
        }
        qDebug("found? %d", found);
        qDebug("index = %d", index);
        QString name = info.mid(name_starts_at, index-name_starts_at);
        int ignore_mpc = name.lastIndexOf(" - Media Player Classic Home Cinema - v");
        if( ignore_mpc != -1 ) {
            // for earlier versions of MPC, e.g., v1.4.2677.0
            name.truncate(ignore_mpc);
        }
        name.replace("\\'", "'"); // replace \' with '
        this->nameWidget->setText(name);

        //QByteArray remainder = info.mid(index);
        QString remainder = info.mid(index);
        // Once More, With Feeling:
        // we have to do the filename separately and chop it off, to avoid problems caused by filenames with commas
        //QList<QByteArray> infoList = remainder.split(',');
        QList<QString> infoList = remainder.split(',');

        if( infoList.size() < 3 ) {
            return;
        }
        QString time_str = infoList[3];
        {
            time_str = time_str.mid(2); // get rid of space and first quote
            time_str.chop(1); // get rid of last quote
        }
        QTime new_time = QTime::fromString(time_str, "hh:mm:ss");
        if( have_saved_last_time ) {
            if( new_time == saved_last_time ) {
                // probably paused?
                is_paused = true;
                qDebug("paused?");
            }
            else {
                // assume not paused
                is_paused = false;
                qDebug("playing?");
                //qDebug("%s, %s", time.toString("hh:mm:ss.zzz").toStdString().c_str(), new_time.toString("hh:mm:ss.zzz").toStdString().c_str());
            }
        }
        time = new_time;
        have_time = true;
        saved_last_time = time;
        have_saved_last_time = true;
        time_last_updated.start();
        this->refreshTimeLabel();

        if( infoList.size() < 5 ) {
            return;
        }
        QString length_str = infoList[5];
        {
            length_str = length_str.mid(2); // get rid of space first quote
            length_str.chop(1); // get rid of last quote
        }
        lengthLabel->setText("/ " + length_str);
        QTime length = QTime::fromString(length_str, "hh:mm:ss");
        int length_seconds = - length.secsTo(QTime(0, 0, 0, 0));
        //slider->setMaximum(length_seconds);
        progress->setMaximum(length_seconds);
        qDebug("length: %d", length_seconds);
    }
    else if( player == PLAYER_VLC ) {
        QByteArray data = reply->readAll(); // force us to read all the data
        //qDebug("received: %s", data.data());
        QXmlStreamReader reader(data);

        this->nameWidget->setText("");
        this->timeLabel->setText("");
        this->lengthLabel->setText("");
        /*if( !this->slider_pressed ) {
            this->slider->setValue(0);
        }*/
        this->progress->setValue(0);

        qDebug("parsing xml...");
        //QString text_artist, text_title;
        //bool in_meta_information = false;
        while( !reader.atEnd() ) {
            reader.readNext();
            /*if( reader.name() == "meta-information" ) {
                // v1 API
                if( reader.isStartElement() ) {
                    qDebug("now reading meta information [v1]:");
                    in_meta_information = true;
                }
                else if( reader.isEndElement() ) {
                    qDebug("done reading meta information! [v1]");
                    in_meta_information = false;
                }
            }
            else if( reader.isStartElement() && reader.name() == "category" && reader.attributes().value("name") == "meta" ) {
                // v2 API
                qDebug("now reading meta information [v2]:");
                in_meta_information = true;
            }
            else if( reader.isEndElement() && reader.name() == "category" && reader.attributes().value("name") == "meta" ) {
                // v2 API
                qDebug("now reading meta information [v2]:");
                in_meta_information = true;
            }
            else*/ if( reader.isStartElement() ) {
                qDebug("read: %s", reader.name().toString().toStdString().c_str());
                if( reader.name() == "volume" ) {
                    QString element = reader.readElementText();
                    qDebug("volume = %s", element.toStdString().c_str());
                    bool ok = false;
                    int volume = element.toInt(&ok);
                    if( ok && volume > 0 ) {
                        saved_volume = volume;
                        qDebug("saved volume %d", saved_volume);
                    }
                }
                else if( reader.name() == "state" ) {
                    QString element = reader.readElementText();
                    qDebug("state = %s", element.toStdString().c_str());
                    is_paused = element != "playing";
                }
                else if( reader.name() == "length" ) {
                    QString element = reader.readElementText();
                    qDebug("length = %s", element.toStdString().c_str());
                    QString length_seconds_str = element;
                    bool ok = false;
                    int length_seconds = length_seconds_str.toInt(&ok);
                    if( ok ) {
                        qDebug("length seconds = %d", length_seconds);
                        QTime length(0, 0);
                        length = length.addSecs(length_seconds);
                        QString length_str = length.toString("hh:mm:ss");
                        qDebug("length_str = %s", length_str.toStdString().c_str());
                        lengthLabel->setText("/ " + length_str);
                        //slider->setMaximum(length_seconds);
                        progress->setMaximum(length_seconds);
                    }
                }
                else if( reader.name() == "time" ) {
                    QString element = reader.readElementText();
                    qDebug("time = %s", element.toStdString().c_str());
                    QString time_seconds_str = element;
                    bool ok = false;
                    int time_seconds = time_seconds_str.toInt(&ok);
                    if( ok ) {
                        qDebug("time seconds = %d", time_seconds);
                        QTime new_time(0, 0);
                        new_time = new_time.addSecs(time_seconds);
                        time = new_time;
                        have_time = true;
                        saved_last_time = time;
                        have_saved_last_time = true;
                        time_last_updated.start();
                        this->refreshTimeLabel();
                    }
                }
                else if( reader.name() == "information" ) {
                    this->parseStatusInformation(reader);
                }
                /*else if( in_meta_information && reader.name() == "title" ) {
                    QString element = reader.readElementText();
                    qDebug("title = %s", element.toStdString().c_str());
                    // need to decode HTML "&" encoding etc
                    QTextDocument document;
                    document.setHtml(element);
                    text_title = document.toPlainText();
                }
                else if( in_meta_information && reader.name() == "artist" ) {
                    QString element = reader.readElementText();
                    qDebug("artist = %s", element.toStdString().c_str());
                    // need to decode HTML "&" encoding etc
                    QTextDocument document;
                    document.setHtml(element);
                    text_artist = document.toPlainText();
                }*/
            }
        }
        /*QString text_name;
        if( text_artist.length() > 0 ) {
            text_name += text_artist;
        }
        if( text_title.length() > 0 ) {
            if( text_name.length() > 0 ) {
                text_name += " - ";
            }
            text_name += text_title;
        }
        this->nameWidget->setText(text_name);*/
    }

    reply->deleteLater();
}

void MainWindow::openSettingsWindow() {
    qDebug("openSettingsWindow");
    if( settingsWindow != NULL ) {
        return;
    }
    qDebug("opening settings window");
    settingsWindow = new SettingsWindow(this);

    settingsWindow->setFont(this->font());

    QVBoxLayout *layout = new QVBoxLayout();
    settingsWindow->setLayout(layout);

    {
        QWebView *helpInfo = new QWebView();
        this->webViewEventFilter->setWebView(helpInfo); // on Android at least, need to disable selection to be able to click help link!
        if( player == PLAYER_MPC ) {
            helpInfo->setHtml("<html><body><p>Please make sure that Media Player Classic is running on your PC, and the Web Interface enabled (go to menu View/Options, then under Player/Web Interface tick \"Listen on port\"). The <b>IP Address</b> can be found by running ipconfig at a command prompt on the PC. It will take the form of 4 numbers separated by periods, e.g., 192.168.1.1. Please see <a href=\"http://homepage.ntlworld.com/mark.harman/comp_wifiremoteplay.html\">the online help</a> for more details. <b>Port</b> should be left at 13579, unless you have changed it in the MPC options.</p></body></html>");
        }
        else if( player == PLAYER_VLC ) {
            helpInfo->setHtml("<html><body><p>Please make sure that VLC is running on your PC, and the Web Interface enabled, and the .hosts file edited - please see <a href=\"http://homepage.ntlworld.com/mark.harman/comp_wifiremoteplay.html\">the online help</a> for details. The <b>IP Address</b> can be found by running ipconfig at a command prompt on the PC. It will take the form of 4 numbers separated by periods, e.g., 192.168.1.1. <b>Port</b> should be left at 8080, unless you have changed it in the VLC options.</p></body></html>");
        }
#if defined(Q_OS_ANDROID)
        helpInfo->setMaximumHeight(this->height()/2); // workaround on Android, where hard to click the Okay/Cancel buttons at bottom of screen, so need this to make the buttons have more space
#endif
        layout->addWidget(helpInfo);
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *ipAddressLabel = new QLabel(tr("IP Address"));
        h_layout->addWidget(ipAddressLabel);

        ipAddressLineEdit = new QLineEdit(this->ip_address);
        h_layout->addWidget(ipAddressLineEdit);
#if !defined(Q_OS_ANDROID)
        // on Android, the virtual keyboard doesn't have arrow keys, so putting in the periods makes things more of a hassle.
        ipAddressLineEdit->setInputMask("000.000.000.000");
#endif
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *portLabel = new QLabel(tr("Port"));
        h_layout->addWidget(portLabel);

        int port = 0;
        if( player == PLAYER_MPC ) {
            port = this->mpc_port;
        }
        else if( player == PLAYER_VLC ) {
            port = this->vlc_port;
        }
        portLineEdit = new QLineEdit(QString::number(port));
        h_layout->addWidget(portLineEdit);
        portLineEdit->setInputMask("00000");
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *okaySettingsButton = new QPushButton(tr("Okay"));
        initButton(okaySettingsButton);
        h_layout->addWidget(okaySettingsButton);
        connect(okaySettingsButton, SIGNAL(clicked()), this, SLOT(clickedSettingsOkay()));

        if( ip_address != "" ) {
            QPushButton *cancelSettingsButton = new QPushButton(tr("Cancel"));
            initButton(cancelSettingsButton);
            h_layout->addWidget(cancelSettingsButton);
            connect(cancelSettingsButton, SIGNAL(clicked()), this, SLOT(clickedSettingsCancel()));
        }
    }

    /*{
        QMenuBar *menu = settingsWindow->menuBar();
        QAction *helpAction = new QAction(tr("Online help"), this);
        connect(helpAction, SIGNAL(triggered()), this, SLOT(help()));
        menu->addAction(helpAction);
    }*/

    settingsWindow->setAttribute(Qt::WA_DeleteOnClose);
    settingsWindow->showMaximized();
    this->hide();
    settingsWindow->activateWindow(); // needed on Android otherwise back button closes MainWindow, when settings window appears upon startup
}

void MainWindow::clickedSettingsOkay() {
    qDebug("clickedSettingsOkay");
    ip_address = ipAddressLineEdit->text();
    settings->setValue(ip_address_key_c, ip_address);
    qDebug("ip_address is now: %s", ip_address.toStdString().c_str());

    bool ok = false;
    int port = portLineEdit->text().toInt(&ok);
    if( player == PLAYER_MPC ) {
        if( !ok ) {
            qDebug("failed to parse int");
            // set to default
            port = default_mpc_port_c;
        }
        this->mpc_port = port;
        settings->setValue(mpc_port_key_c, port);
    }
    else if( player == PLAYER_VLC ) {
        if( !ok ) {
            qDebug("failed to parse int");
            // set to default
            port = default_vlc_port_c;
        }
        this->vlc_port = port;
        settings->setValue(vlc_port_key_c, port);
    }
    qDebug("port is now: %d", port);

    this->show();
    delete settingsWindow;
    settingsWindow = NULL;
    this->activateWindow(); // needed for Android at least
    this->waiting_for_status = false; // ignore any previous requests
    this->requestStatus();
}

void MainWindow::clickedSettingsCancel() {
    qDebug("clickedSettingsCancel");
    this->show();
    delete settingsWindow;
    settingsWindow = NULL;
    this->activateWindow(); // needed for Android at least
}

void MainWindow::openPlayerWindow() {
    qDebug("openPlayerWindow");
    if( playerWindow != NULL ) {
        return;
    }
    qDebug("opening player window");
    playerWindow = new UncloseWindow(); // disable back button on Android - player must explicitly choose a player

    QFont new_font = this->font();
    new_font.setPointSize( new_font.pointSize() + 2 );
    playerWindow->setFont(this->font());

    QVBoxLayout *layout = new QVBoxLayout();
    playerWindow->setLayout(layout);

    /*QPalette palette = settingsWindow->palette();
    //palette.setColor(QPalette::Window, Qt::white);
    //palette.setColor(QPalette::WindowText, Qt::black);
    palette.setColor(QPalette::Window, Qt::black);
    palette.setColor(QPalette::WindowText, Qt::white);
    //palette.setColor(QPalette::Button, Qt::black);
    //palette.setColor(QPalette::ButtonText, Qt::white);
    settingsWindow->setPalette(palette);*/
    /*QWebView *label = new QWebView();
    label->setHtml("<html><body><p>Please select which media player you want to use with Wifi Remote Play:</p></body></html>");*/
    QLabel *label = new QLabel("Please select which media player you want to use with Wifi Remote Play:");
    label->setWordWrap(true);
    layout->addWidget(label);

    QString mpc_text = "Media Player Classic";
    QString vlc_text = "VLC";
    if( settings->contains(player_key_c) ) {
        if( player == PLAYER_MPC ) {
            mpc_text += " (Current)";
        }
        else if( player == PLAYER_VLC ) {
            vlc_text += " (Current)";
        }
    }

    {
        QPushButton *MPCPlayerButton = new QPushButton(mpc_text);
        initButton(MPCPlayerButton);
        layout->addWidget(MPCPlayerButton);
        connect(MPCPlayerButton, SIGNAL(clicked()), this, SLOT(clickedMPCPlayer()));
    }

    {
        QPushButton *VLCPlayerButton = new QPushButton(vlc_text);
        initButton(VLCPlayerButton);
        layout->addWidget(VLCPlayerButton);
        connect(VLCPlayerButton, SIGNAL(clicked()), this, SLOT(clickedVLCPlayer()));
    }

    playerWindow->setAttribute(Qt::WA_DeleteOnClose);
    playerWindow->showMaximized();
    this->hide();
}

void MainWindow::clickedMPCPlayer() {
    qDebug("clickedMPCPlayer");
    this->player = PLAYER_MPC;
    settings->setValue(player_key_c, player);
    qDebug("player is now: %d", player);

    this->createUI();
    this->show();
    delete playerWindow;
    playerWindow = NULL;
    this->activateWindow(); // needed for Android at least
    this->waiting_for_status = false; // ignore any previous requests
    this->requestStatus();

    if( !this->IPDefined() ) {
        this->openSettingsWindow();
    }
}

void MainWindow::clickedVLCPlayer() {
    qDebug("clickedVLCPlayer");
    this->player = PLAYER_VLC;
    settings->setValue(player_key_c, player);
    qDebug("player is now: %d", player);

    this->createUI();
    this->show();
    delete playerWindow;
    playerWindow = NULL;
    this->activateWindow(); // needed for Android at least
    this->waiting_for_status = false; // ignore any previous requests
    this->requestStatus();

    if( !this->IPDefined() ) {
        this->openSettingsWindow();
    }
}

void MainWindow::openDVDWindow() {
    qDebug("openDVDWindow");
    if( dvdWindow != NULL ) {
        return;
    }

    qDebug("opening dvd window");
    dvdWindow = new DVDWindow(this);
    dvdWindow->setFont(this->font());
    dvdWindow->setPalette(this->palette());

    QGridLayout *layout = new QGridLayout();
    dvdWindow->setLayout(layout);

    QPushButton *upDVDButton = new QPushButton(tr("Up"));
    layout->addWidget(upDVDButton, 0, 1);
    connect(upDVDButton, SIGNAL(clicked()), this, SLOT(clickedDVDUp()));

    QPushButton *leftDVDButton = new QPushButton(tr("Left"));
    layout->addWidget(leftDVDButton, 1, 0);
    connect(leftDVDButton, SIGNAL(clicked()), this, SLOT(clickedDVDLeft()));

    QPushButton *activateDVDButton = new QPushButton(tr("Activate"));
    layout->addWidget(activateDVDButton, 1, 1);
    connect(activateDVDButton, SIGNAL(clicked()), this, SLOT(clickedDVDActivate()));

    QPushButton *rightDVDButton = new QPushButton(tr("Right"));
    layout->addWidget(rightDVDButton, 1, 2);
    connect(rightDVDButton, SIGNAL(clicked()), this, SLOT(clickedDVDRight()));

    QPushButton *backDVDButton = new QPushButton(tr("Back"));
    layout->addWidget(backDVDButton, 2, 0);
    connect(backDVDButton, SIGNAL(clicked()), this, SLOT(clickedDVDBack()));

    QPushButton *downDVDButton = new QPushButton(tr("Down"));
    layout->addWidget(downDVDButton, 2, 1);
    connect(downDVDButton, SIGNAL(clicked()), this, SLOT(clickedDVDDown()));

    QPushButton *leaveDVDButton = new QPushButton(tr("Leave"));
    layout->addWidget(leaveDVDButton, 2, 2);
    connect(leaveDVDButton, SIGNAL(clicked()), this, SLOT(clickedDVDLeave()));

    QPushButton *closeDVDButton = new QPushButton(tr("Close"));
    layout->addWidget(closeDVDButton, 3, 1);
    connect(closeDVDButton, SIGNAL(clicked()), this, SLOT(clickedDVDClose()));

    dvdWindow->setAttribute(Qt::WA_DeleteOnClose);
    dvdWindow->showMaximized();
    this->hide();
}

void MainWindow::clickedDVDUp() {
    qDebug("clickedDVDUp");
    if( player == PLAYER_MPC ) {
        sendMPCCommand(931);
    }
}

void MainWindow::clickedDVDDown() {
    qDebug("clickedDVDDown");
    if( player == PLAYER_MPC ) {
        sendMPCCommand(932);
    }
}

void MainWindow::clickedDVDLeft() {
    qDebug("clickedDVDLeft");
    if( player == PLAYER_MPC ) {
        sendMPCCommand(929);
    }
}

void MainWindow::clickedDVDRight() {
    qDebug("clickedDVDRight");
    if( player == PLAYER_MPC ) {
        sendMPCCommand(930);
    }
}

void MainWindow::clickedDVDActivate() {
    qDebug("clickedDVDActivate");
    if( player == PLAYER_MPC ) {
        sendMPCCommand(933);
    }
}

void MainWindow::clickedDVDBack() {
    qDebug("clickedDVDBack");
    if( player == PLAYER_MPC ) {
        sendMPCCommand(934);
    }
}

void MainWindow::clickedDVDLeave() {
    qDebug("clickedDVDLeave");
    if( player == PLAYER_MPC ) {
        sendMPCCommand(935);
    }
}

void MainWindow::clickedDVDClose() {
    qDebug("clickedDVDClose");
    this->show();
    delete dvdWindow;
    dvdWindow = NULL;
    this->activateWindow(); // needed for Android at least
}

void MainWindow::openBrowserWindow() {
    qDebug("openBrowserWindow");
    if( browserWindow != NULL ) {
        return;
    }

    qDebug("opening browser window");
    browserWindow = new BrowserWindow(this);
    browserWindow->setFont(this->font());

    QVBoxLayout *layout = new QVBoxLayout();
    browserWindow->setLayout(layout);

    /*
    QGraphicsScene *scene = new QGraphicsScene();
    QGraphicsView *view = new QGraphicsView(scene);
    view->setFrameShape(QFrame::NoFrame);
    //view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    //view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setDragMode(QGraphicsView::ScrollHandDrag);

    QGraphicsWebView *webview = new QGraphicsWebView();
    //webview->resize(this->width(), this->height());
    webview->resize(this->width(), this->height());
    webview->load(QUrl("http://doc.qt.nokia.com/"));
    webview->setAttribute(Qt::WA_TransparentForMouseEvents);
    MyEventFilter *eventFilter = new MyEventFilter(this);
    //webview->installEventFilter(eventFilter);
    //webview->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
    webview->setZValue(-1.0);

    //scene->addItem(webview);

    //view->resize(width, height);
    //view->show();
    layout->addWidget(view);

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *closeBrowserButton = new QPushButton(tr("Close"));
        h_layout->addWidget(closeBrowserButton);
        connect(closeBrowserButton, SIGNAL(clicked()), this, SLOT(clickedBrowserClose()));

    }

    browserWindow->setAttribute(Qt::WA_DeleteOnClose);
    browserWindow->showMaximized();
    //webview->resize(browserWindow->width(), browserWindow->height());
    //webview->resize(view->width(), view->height());
    webview->setResizesToContents(true);
    QBrush brush(Qt::red);
    scene->addRect(webview->rect(), Qt::NoPen, brush);
    scene->setSceneRect(0.0, 0.0, 10000.0, 10000.0);
    this->hide();

    return;*/

    this->webView = new QWebView();
    layout->addWidget(webView);
#if defined(Q_OS_ANDROID)
    this->webView->setTextSizeMultiplier(2.0); // to make the links easier to select on phone (seems better default on Galaxy Nexus at least)
#else
    this->webView->setTextSizeMultiplier(1.5); // to make the links easier to select on phone
#endif
    this->webViewEventFilter->setWebView(this->webView);
    this->webView->setContextMenuPolicy(Qt::NoContextMenu); // don't want context menu, as options not relevant, or don't work properly with the hackery that we're doing
    connect(this->webView, SIGNAL(loadFinished(bool)), this, SLOT(loadFinished()));
    this->webView->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    connect(this->webView, SIGNAL(linkClicked(const QUrl&)), this, SLOT(linkClicked(const QUrl&)));

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *zoomOutButton = new QPushButton(tr("-"));
        h_layout->addWidget(zoomOutButton);
        connect(zoomOutButton, SIGNAL(clicked()), this, SLOT(clickedBrowserZoomOut()));

        QPushButton *zoomInButton = new QPushButton(tr("+"));
        h_layout->addWidget(zoomInButton);
        connect(zoomInButton, SIGNAL(clicked()), this, SLOT(clickedBrowserZoomIn()));

        QPushButton *closeBrowserButton = new QPushButton(tr("Close"));
        h_layout->addWidget(closeBrowserButton);
        connect(closeBrowserButton, SIGNAL(clicked()), this, SLOT(clickedBrowserClose()));

    }

    int port = 0;
    if( player == PLAYER_MPC ) {
        port = this->mpc_port;
    }
    else if( player == PLAYER_VLC ) {
        port = this->vlc_port;
    }
    QString url = "http://" + this->ip_address + ":" + QString::number(port) + "/browser.html";
    qDebug("load: %s", url.toStdString().c_str());
    done_html_modification = false;
    webView->load(url);
    browserWindow->setAttribute(Qt::WA_DeleteOnClose);
    browserWindow->showMaximized();
    this->hide();

}

void MainWindow::loadFinished() {
    qDebug("loadFinished");
    if( browserWindow == NULL ) {
        // already closed?!
        return;
    }
    // We do some hackery here, to replace the ".." used to indicate the parent directory link. Firstly, ".."
    // is not very user friendly. Secondly, it means that the link is very small, and difficult to select on
    // a phone (especially without a stylus).
    if( done_html_modification ) {
        // to avoid calling this again (theoretically we shouldn't get an infinite loop, as the modification will fail on the second loop, but this is just to be safe)
        return;
    }
    done_html_modification = true;
    QString html = this->webView->page()->mainFrame()->toHtml();
    int index_dir = html.indexOf("<tr class=\"dir\">");
    if( index_dir != -1 ) {
        int index_directory = html.indexOf("<td class=\"dirtype\">Directory</td>", index_dir+1);
        if( index_directory != 1 ) {
            // Theoretically searching for ".." should find the link for parent directory, but there's a risk
            // of this breaking if it's changed in future Media Player Classic versions, which would result in
            // problems if any filenames had ".." in the title! So we use these search terms to narrow down where
            // we want to look.
            int index_parent = html.indexOf(">..</a>", index_dir+1);
            if( index_parent != -1 && index_parent < index_directory ) {
                index_parent++;
                html.replace(index_parent, 2, "Parent Directory");
                int port = 0;
                if( player == PLAYER_MPC ) {
                    port = this->mpc_port;
                }
                else if( player == PLAYER_VLC ) {
                    port = this->vlc_port;
                }
                QString url = "http://" + this->ip_address + ":" + QString::number(port) + "/browser.html";
                qDebug("count before: %d", this->webView->history()->count());
                this->webView->setHtml(html, url);
                qDebug("count after: %d", this->webView->history()->count());
                /*QString html2 = this->webView->page()->mainFrame()->toHtml();
                qDebug("%s", html2.toStdString().c_str());*/
            }
        }
    }
    //this->webView->setHtml(webViewContent);
}

void MainWindow::linkClicked(const QUrl &url) {
    qDebug("linkClicked");
    qDebug("url: %s", url.encodedPath().data());
    qDebug("query: %s", url.encodedQuery().data());
    this->done_html_modification = false; // need to set, so that we process the new page
    this->webView->load(url); // need to load manually, as we're handling the links
}

const float zoom_factor = 1.2;

void MainWindow::clickedBrowserZoomOut() {
    qDebug("clickedBrowserZoomOut");
    if( this->webView != NULL ) {
        float zoom = this->webView->textSizeMultiplier();
        zoom /= zoom_factor;
        qDebug("zoom now: %f", zoom);
        this->webView->setTextSizeMultiplier(zoom);
    }
}

void MainWindow::clickedBrowserZoomIn() {
    qDebug("clickedBrowserZoomIn");
    if( this->webView != NULL ) {
        float zoom = this->webView->textSizeMultiplier();
        zoom *= zoom_factor;
        qDebug("zoom now: %f", zoom);
        this->webView->setTextSizeMultiplier(zoom);
    }
}

void MainWindow::clickedBrowserClose() {
    qDebug("clickedBrowserClose");
    this->show();
    delete browserWindow;
    browserWindow = NULL;
    this->activateWindow(); // needed for Android at least
    this->requestStatus(); // in case a new file selected

    /*
        // test code for going back - doesn't work due to problems in Qt, doesn't go back when page modified with setHtml()!
    this->done_html_modification = false; // need to set, so that we process the new page
    //webView->back();
    //webView->page()->triggerAction(QWebPage::Back);
    //webView->triggerPageAction(QWebPage::Back);
    //webView->page()->triggerAction(QWebPage::Reload);
    if( this->webView->history()->count() > 0 ) {
        QWebHistoryItem backItem = this->webView->history()->backItem();
        QString fixedUrl = QUrl::fromPercentEncoding(backItem.url().toString().toAscii());
        qDebug("back URL: %s", backItem.url().toString().toStdString().c_str());
        qDebug("fixed: %s", fixedUrl.toStdString().c_str());
        this->webView->load(fixedUrl);
        this->webView->history()->goToItem(backItem);
    }
    else {
        qDebug("can't go back");
    }
    */
}

void MainWindow::toggleFullscreen() {
    qDebug("toggleFullscreen");
    if( player == PLAYER_MPC ) {
        sendMPCCommand(830);
    }
    else if( player == PLAYER_VLC ) {
        sendVLCCommand("fullscreen");
    }
    this->activateWindow(); // needed for Android at least
}

void MainWindow::about() {
    qDebug("about");
    QString text = tr("Wifi Remote Play v1.9\nby Mark Harman\nPlease donate if you like this app :)");
    QMessageBox::about(this, tr("About Wifi Remote Play"), text);
    this->activateWindow(); // needed for Android at least
}

void MainWindow::help() {
    qDebug("help");
    QDesktopServices::openUrl(QUrl("http://homepage.ntlworld.com/mark.harman/comp_wifiremoteplay.html"));
    this->activateWindow(); // needed for Android at least
}

void MainWindow::donate() {
    qDebug("donate");
    QDesktopServices::openUrl(QUrl("http://sourceforge.net/donate/?user_id=439017"));
    this->activateWindow(); // needed for Android at least
}

void MainWindow::webiface() {
    qDebug("webiface");
    if( !this->IPDefined() ) {
        qDebug("invalid ip");
        return;
    }
    if( player == PLAYER_MPC ) {
        QString url = "http://" + this->ip_address + ":" + QString::number(mpc_port) + "/controls.html";
        qDebug("open: %s", url.toStdString().c_str());
        QDesktopServices::openUrl(QUrl(url));
    }
    else if( player == PLAYER_VLC ) {
        QString url = "http://" + this->ip_address + ":" + QString::number(vlc_port) + "/";
        qDebug("open: %s", url.toStdString().c_str());
        QDesktopServices::openUrl(QUrl(url));
    }
    this->activateWindow(); // needed for Android at least
}
