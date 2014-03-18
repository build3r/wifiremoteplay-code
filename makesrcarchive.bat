rd /S /Q c:\temp\wifiremoteplaysrc\

mkdir c:\temp\wifiremoteplaysrc\

set src="."
set dst="c:\temp\wifiremoteplaysrc"

copy %src%\makesrcarchive.bat %dst%
copy %src%\wifiremoteplay_source.txt %dst%
copy %src%\gpl-3.0.txt %dst%

copy %src%\WifiRemotePlay.pro %dst%
copy %src%\deployment.pri %dst%

copy %src%\icon.svg %dst%
copy %src%\icon.png %dst%

copy %src%\main.cpp %dst%
copy %src%\main.h %dst%
copy %src%\mainwindow.cpp %dst%
copy %src%\mainwindow.h %dst%
copy %src%\namewidget.cpp %dst%
copy %src%\namewidget.h %dst%

mkdir %dst%\android
xcopy %src%\android %dst%\android /E /Y

REM exit
