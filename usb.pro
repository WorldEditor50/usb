TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        hid.cpp \
        main.cpp \
        usb.cpp \
        usbasync.cpp

HEADERS += \
    hid.h \
    usb.h \
    usbasync.h

PATH = D:/home/3rdparty
# hid
INCLUDEPATH += $$PATH/hidapi/include
LIBS += -L$$PATH/hidapi/lib -lhidapi
# libusb
INCLUDEPATH += $$PATH/libusb/include
LIBS += -L$$PATH/libusb/static -llibusb-1.0

