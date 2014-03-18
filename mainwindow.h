#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QHostAddress>
#include <QNetworkReply>
#include <QtWebKit/QWebView>

#include "namewidget.h"

class MainWindow;

class WebViewEventFilter : public QObject {
    Q_OBJECT

    QWebView *webView;
    bool filterMouseMove;
    int orig_mouse_x, orig_mouse_y;
    int saved_mouse_x, saved_mouse_y;
protected:

    bool eventFilter(QObject *obj, QEvent *event);

public:
    WebViewEventFilter(QObject *parent) : QObject(parent), webView(NULL), filterMouseMove(false), orig_mouse_x(0), orig_mouse_y(0), saved_mouse_x(0), saved_mouse_y(0) {
    }

    void setWebView(QWebView *webView);
};

class TimeProgressBar : public QProgressBar {
    Q_OBJECT

    MainWindow *mainWindow;
    bool mouse_down;

    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);

    void registerClick(int m_x);

public:
    TimeProgressBar(MainWindow *mainWindow) : QProgressBar(), mainWindow(mainWindow), mouse_down(false) {
    }
    virtual ~TimeProgressBar() {
    }
};

class UncloseWindow : public QWidget
{
    Q_OBJECT

protected:
    void closeEvent(QCloseEvent *event) {
        event->ignore();
    }

public:
    UncloseWindow() : QWidget() {
    }
};

class SettingsWindow : public QWidget
{
    Q_OBJECT

    MainWindow *mainWindow;

protected:
    void closeEvent(QCloseEvent *event);

public:
    SettingsWindow(MainWindow *mainWindow) : QWidget(), mainWindow(mainWindow) {
    }
};

class DVDWindow : public QWidget
{
    Q_OBJECT

    MainWindow *mainWindow;

protected:
    void closeEvent(QCloseEvent *event);

public:
    DVDWindow(MainWindow *mainWindow) : QWidget(), mainWindow(mainWindow) {
    }
};

class BrowserWindow : public QWidget
{
    Q_OBJECT

    MainWindow *mainWindow;

protected:
    void closeEvent(QCloseEvent *event);

public:
    BrowserWindow(MainWindow *mainWindow) : QWidget(), mainWindow(mainWindow) {
    }
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

    friend class SettingsWindow;
    friend class DVDWindow;
    friend class BrowserWindow;

    QTimer *timer_request_status;
    QTimer *timer_refresh_time;

    enum Player {
        PLAYER_MPC = 0,
        PLAYER_VLC = 1
    };
    Player player;

    QString ip_address;
    int mpc_port;
    int vlc_port;

    QUrl url;
    QNetworkAccessManager qnam;
    //QNetworkReply *reply;
    QTcpSocket *socket;
    int command;
    bool has_extra;
    QString extra_name;
    QString extra_value;
    bool waiting_for_status;

    bool is_paused;
    bool have_time;
    QTime time;
    bool have_saved_last_time;
    QTime saved_last_time;
    //QTime time_last_updated;
    QElapsedTimer time_last_updated;

    QElapsedTimer status_last_check_timer;

    bool is_muted; // for VLC
    int saved_volume; // for VLC unmuting

    //QLabel *nameLabel;
    NameWidget *nameWidget;
    /*QSlider *slider;
    bool slider_pressed;*/
    QProgressBar *progress;
    QLabel *timeLabel;
    QLabel *lengthLabel;

    QSettings *settings;

    QWidget *settingsWindow;
    //QMainWindow *settingsWindow;
    QLineEdit *ipAddressLineEdit;
    QLineEdit *portLineEdit;

    QWidget *playerWindow;

    QWidget *dvdWindow;

    WebViewEventFilter *webViewEventFilter;
    QWidget *browserWindow;
    QWebView *webView;
    bool done_html_modification; // whether we've called the routine to try to modify the html

    void initButton(QPushButton *button);
    void sendMPCCommand(int command);
    void sendMPCCommand(int command, bool has_extra, QString extra_name, QString extra_value);
    void sendVLCCommand(QString command);
    void createUI();
    void parseStatusMetaInformation(QXmlStreamReader &reader);
    void parseStatusMetaCategory(QXmlStreamReader &reader);
    void parseStatusInformation(QXmlStreamReader &reader);

    //bool eventFilter(QObject *obj, QEvent *event); // used for Application event filter

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void clickedPrev();
    void clickedNext();
    void clickedBwd();
    void clickedFwd();
    void clickedPlay();
    void clickedVolumeMinus();
    void clickedVolumePlus();
    void clickedVolumeMute();
    /*void sliderPressed();
    void sliderReleased();*/

    void refreshTimeLabel();

    void connected();
    //void networkError(QNetworkReply::NetworkError);
    void networkError(QAbstractSocket::SocketError socketError);
    void qnamFinished(QNetworkReply *reply);

    void toggleFullscreen();
    void about();
    void help();
    void donate();
    void webiface();

    void clickedSettingsOkay();
    void clickedSettingsCancel();

    void clickedMPCPlayer();
    void clickedVLCPlayer();

    void clickedDVDUp();
    void clickedDVDDown();
    void clickedDVDLeft();
    void clickedDVDRight();
    void clickedDVDActivate();
    void clickedDVDBack();
    void clickedDVDLeave();
    void clickedDVDClose();

    void loadFinished();
    void linkClicked(const QUrl &url);
    void clickedBrowserZoomOut();
    void clickedBrowserZoomIn();
    void clickedBrowserClose();
public:
    enum ScreenOrientation {
        ScreenOrientationLockPortrait,
        ScreenOrientationLockLandscape,
        ScreenOrientationAuto
    };

    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    // Note that this will only have an effect on Symbian and Fremantle.
    void setOrientation(ScreenOrientation orientation);

    void showExpanded();

    bool IPDefined() {
        if( this->ip_address.length() < 4 || this->ip_address.contains("..") || this->ip_address.startsWith('.') || this->ip_address.endsWith('.') )
            return false;
        return true;
    }
    void activateTimers(bool activate) {
        if( timer_request_status != NULL ) {
            activate ? timer_request_status->start() : timer_request_status->stop();
        }
        if( timer_refresh_time != NULL ) {
            activate ? timer_refresh_time->start() : timer_refresh_time->stop();
        }
        if( nameWidget != NULL ) {
            nameWidget->activateTimers(activate);
        }
    }
    void moveToTime(int slider_time);

public slots:
    void openSettingsWindow();
    void openPlayerWindow();
    void openDVDWindow();
    void openBrowserWindow();
    void requestStatus();
};

#endif // MAINWINDOW_H
