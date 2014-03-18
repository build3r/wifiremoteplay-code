#ifndef MAIN_H
#define MAIN_H

#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!
#include <QtGui/QApplication>

class MainWindow;

class MyApplication : public QApplication {

protected:
    bool is_active;
    MainWindow *mainWindow;

#ifndef Q_OS_ANDROID
    // going inactive doesn't seem to work on Android Qt!
    bool event(QEvent *event);
#endif

public:
    MyApplication(int argc, char *argv[]) : QApplication(argc, argv), is_active(true), mainWindow(NULL) {
    }

    void setMainWindow(MainWindow *mainWindow) {
        this->mainWindow = mainWindow;
    }
    bool isActive() const {
        return is_active;
    }
    void activate(bool active);
};

extern MyApplication *myApp;

#endif // MAIN_H
