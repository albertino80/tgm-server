QT -= gui
QT += xml
QT += network

CONFIG += c++11 console
CONFIG -= app_bundle

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += NO_SSL

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        ../civetweb/src/handle_form.inl \
        ../civetweb/src/md5.inl \
        ../civetweb/src/sha1.inl \
        ../civetweb/src/timer.inl \
        ../civetweb/src/civetweb.c \
        ../civetweb/src/CivetServer.cpp \
        main_server.cpp \
        routeshandler.cpp \
        telegramhelper.cpp \
        telegramwebhook.cpp

INCLUDEPATH += $$PWD/../civetweb/include

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
	routeshandler.h \
	telegramhelper.h \
	telegramwebhook.h
