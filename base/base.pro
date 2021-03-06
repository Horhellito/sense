TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

TARGET = base
target.path = .
INSTALLS = target

rpi {
    DEFINES+=RASPBERRY_PI
    CONFIG(debug, debug|release) {
        DEST_FOLDER = rpi/debug
    }
    CONFIG(release, debug|release) {
        DEST_FOLDER = rpi/release
        DEFINES += NDEBUG
    }
} else {
    CONFIG(debug, debug|release) {
        DEST_FOLDER = pc/debug
    }
    CONFIG(release, debug|release) {
        DEST_FOLDER = pc/release
        DEFINES += NDEBUG
    }
}

OBJECTS_DIR = ./.obj/$${DEST_FOLDER}
MOC_DIR = ./.moc/$${DEST_FOLDER}
RCC_DIR = ./.rcc/$${DEST_FOLDER}
UI_DIR = ./.ui/$${DEST_FOLDER}
DESTDIR = ../../bin/$${DEST_FOLDER}

INCLUDEPATH += ../common/src
INCLUDEPATH += ../common/src/rfm22b


SOURCES += src/main.cpp \
    ../common/src/Comms.cpp \
    ../common/src/CRC.cpp \
    ../common/src/Storage.cpp \
    src/tests.cpp \
    src/Scheduler.cpp \
    src/DB.cpp \
    ../common/src/rfm22b/rfm22b.cpp \
    ../common/src/rfm22b/rfm22b_spi.cpp \
    src/Admin.cpp \
    src/Sensors_DB.cpp

HEADERS += \
    ../common/src/Comms.h \
    ../common/src/CRC.h \
    ../common/src/Data_Defs.h \
    ../common/src/Storage.h \
    src/Scheduler.h \
    src/DB.h \
    ../common/src/rfm22b/rfm22b.h \
    ../common/src/rfm22b/rfm22b_enums.h \
    ../common/src/rfm22b/rfm22b_spi.h \
    src/Admin.h \
    src/Sensors_DB.h

DISTFILES +=

LIBS += -lboost_thread -lboost_system -lpthread -lmysqlpp -lmysqlclient -lpigpio
