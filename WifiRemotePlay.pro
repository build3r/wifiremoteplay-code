# Add files and directories to ship with the application 
# by adapting the examples below.
# file1.source = myfile
# dir1.source = mydir
DEPLOYMENTFOLDERS = # file1 dir1

QT += network
QT += webkit
QT += xml

# Symbian Signed:
#symbian:TARGET.UID3 = 0xA001052E
# For use with Nokia Ovi Store only:
symbian:TARGET.UID3 = 0x2005fff5

# workaround for Nokia Ovi UID fuckup:
symbian:TARGET = WifiRemotePlay_2005FFF5
symbian:DEPLOYMENT.display_name = WifiRemotePlay

# Smart Installer package's UID
# This UID is from the protected range 
# and therefore the package will fail to install if self-signed
# By default qmake uses the unprotected range value if unprotected UID is defined for the application
# and 0x2002CCCF value if protected UID is given to the application
#symbian:DEPLOYMENT.installer_header = 0x2002CCCF

# Allow network access on Symbian
# SwEvent needed so that lauching the online help still works on Symbian when browser already open
symbian:TARGET.CAPABILITY += NetworkServices SwEvent

# If your application uses the Qt Mobility libraries, uncomment
# the following lines and add the respective components to the 
# MOBILITY variable. 
# CONFIG += mobility
# MOBILITY +=

SOURCES += main.cpp mainwindow.cpp namewidget.cpp
HEADERS += main.h mainwindow.h namewidget.h
FORMS +=

VERSION = 1.9 # Also remember to update android/AndroidManifest.xml and MainWindow::about()

symbian {
    vendorinfo = "%{\"Mark Harman\"}" ":\"Mark Harman\""
    vendor_deploy.pkg_prerules += vendorinfo
    DEPLOYMENT += vendor_deploy
}

# Please do not modify the following two lines. Required for deployment.
include(deployment.pri)
qtcAddDeployment()

OTHER_FILES += \
    android/AndroidManifest.xml \
    android/res/drawable/icon.png \
    android/res/drawable/logo.png \
    android/res/drawable-hdpi/icon.png \
    android/res/drawable-ldpi/icon.png \
    android/res/drawable-mdpi/icon.png \
    android/res/layout/splash.xml \
    android/res/values/libs.xml \
    android/res/values/strings.xml \
    android/res/values-de/strings.xml \
    android/res/values-el/strings.xml \
    android/res/values-es/strings.xml \
    android/res/values-et/strings.xml \
    android/res/values-fa/strings.xml \
    android/res/values-fr/strings.xml \
    android/res/values-id/strings.xml \
    android/res/values-it/strings.xml \
    android/res/values-ja/strings.xml \
    android/res/values-ms/strings.xml \
    android/res/values-nb/strings.xml \
    android/res/values-nl/strings.xml \
    android/res/values-pl/strings.xml \
    android/res/values-pt-rBR/strings.xml \
    android/res/values-ro/strings.xml \
    android/res/values-rs/strings.xml \
    android/res/values-ru/strings.xml \
    android/res/values-zh-rCN/strings.xml \
    android/res/values-zh-rTW/strings.xml \
    android/src/org/kde/necessitas/ministro/IMinistro.aidl \
    android/src/org/kde/necessitas/ministro/IMinistroCallback.aidl \
    android/src/org/kde/necessitas/origo/QtActivity.java \
    android/src/org/kde/necessitas/origo/QtApplication.java \
    android/version.xml \
    _docs/comp_wifiremoteplay.html \
    _docs/_gitignore/todo.txt

ICON = icon.svg
