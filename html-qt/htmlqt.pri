include(../common.pri)
INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

QT += core
QT -= gui

htmlqt-uselib:!htmlqt-buildlib {
    LIBS += -L$$HTMLQT_LIBDIR -l$$HTMLQT_LIBNAME
} else {
    SOURCES += $$PWD/htmltokenizer.cpp
    HEADERS += $$PWD/htmltokenizer.h $$PWD/htmltokenizer_p.h $$PWD/htmltokenizerinterface.h
}

win32 {
    contains(TEMPLATE, lib):contains(CONFIG, shared):DEFINES += QT_HTMLQT_EXPORT
    else:htmlqt-uselib:DEFINES += QT_HTMLQT_IMPORT
}
