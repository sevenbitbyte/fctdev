#-------------------------------------------------
#
# Project created by QtCreator 2011-06-24T14:06:22
#
#-------------------------------------------------

QT       += core gui

TARGET = NetworkDesigner
TEMPLATE = app


#Place all dynamically generated files in appropriate build directory
OBJECTS_DIR = build/obj
MOC_DIR = build/moc
UI_DIR = build/ui

#QMAKE_LINK = $$QMAKE_LINK_C


CONFIG += qt

LIBS += -L ../../src -l switched

#LIBS += -L ../../src/
#LIBS += -l switched iniparser

RESOURCES = media.qrc

SOURCES += src/main.cpp src/mainwindow.cpp \
	src/iniparser.cpp \
	src/dictionary.cpp \
    src/netnode.cpp \
    src/netlink.cpp \
	src/networkdesc.cpp \
    src/networkgraphics.cpp


HEADERS  += src/mainwindow.h \
    src/netnode.h \
    src/netlink.h \
	src/networkdesc.h \
	src/iniparser.h \
	src/dictionary.h \
    src/networkgraphics.h

FORMS    += ui/mainwindow.ui

INCLUDEPATH += ./ src/ ../../src
