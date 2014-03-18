#include "main.h"
#include "mainwindow.h"

MyApplication *myApp = NULL;

void MyApplication::activate(bool active) {
    if( active ) {
        qDebug("application activated");
        //this->exit();
        //this->beep();
        is_active = true;
        if( mainWindow != NULL ) {
            mainWindow->activateTimers(true);
        }
    }
    else {
        qDebug("application deactivated");
        //this->beep();
        is_active = false;
        if( mainWindow != NULL ) {
            mainWindow->activateTimers(false);
        }
    }
}

#ifndef Q_OS_ANDROID
bool MyApplication::event(QEvent *event) {
    if( event->type() == QEvent::ApplicationActivate ) {
        this->activate(true);
    }
    else if( event->type() == QEvent::ApplicationDeactivate ) {
        this->activate(false);
    }
    return false;
}
#endif

#if defined(Q_OS_ANDROID)

// native methods for Android
// see http://community.kde.org/Necessitas/JNI

#include <jni.h>

static void AndroidOnResume(JNIEnv * /*env*/, jobject /*thiz*/) {
    if( myApp != NULL ) {
        myApp->activate(true);
    }
}

static void AndroidOnPause(JNIEnv * /*env*/, jobject /*thiz*/) {
    if( myApp != NULL ) {
        myApp->activate(false);
    }
}

static JNINativeMethod methods[] = {
    {"AndroidOnResume", "()V", (void *)AndroidOnResume},
    {"AndroidOnPause", "()V", (void *)AndroidOnPause}
};

// this method is called immediately after the module is load
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    qDebug("JNI_OnLoad enter");
    JNIEnv* env = NULL;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        qDebug("JNI: can't get environment");
        return -1;
    }

    // search for our class
    jclass clazz=env->FindClass("org/kde/necessitas/origo/QtActivity");
    if (!clazz)
    {
        qDebug("JNI: can't find QtActivity class");
        return -1;
    }
    // keep a global reference to it
    jclass classID = (jclass)env->NewGlobalRef(clazz);

    // register our native methods
    if (env->RegisterNatives(classID, methods, sizeof(methods) / sizeof(methods[0])) < 0)
    {
        qDebug("JNI: failed to register methods");
        return -1;
    }

    qDebug("JNI_OnLoad enter");
    return JNI_VERSION_1_6;
}
#endif

int main(int argc, char *argv[])
{
    //QApplication a(argc, argv);
    MyApplication a(argc, argv);
    ::myApp = &a;

#if defined(Q_OS_ANDROID)
    QFont boldFont = a.font();
    boldFont.setBold(true);
    a.setFont(boldFont);
#endif

    MainWindow mainWindow;
    a.setMainWindow(&mainWindow);
    mainWindow.setOrientation(MainWindow::ScreenOrientationAuto);
    mainWindow.showExpanded();
    if( !mainWindow.IPDefined() ) {
        //mainWindow.openSettingsWindow();
        mainWindow.openPlayerWindow();
    }
    else {
        mainWindow.requestStatus();
    }

    return a.exec();
}
