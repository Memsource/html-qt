include(../common.pri)
INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
#QT *= network
#greaterThan(QT_MAJOR_VERSION, 4): QT *= widgets

message("htmlqt.pri")
message($$HTMLQT_LIBDIR)
message($$HTMLQT_LIBNAME)

htmlqt-uselib:!htmlqt-buildlib {
    LIBS += -L$$HTMLQT_LIBDIR -l$$HTMLQT_LIBNAME
} else {
    SOURCES += $$PWD/htmltokenizer.cpp
    HEADERS += $$PWD/htmltokenizer.h $$PWD/htmltokenizer_p.h $$PWD/htmltokenizerinterface.h
}

win32 {
    message("TEMPLATE:")
    message($$TEMPLATE)
    message("CONFIG:")
    message($$CONFIG)
    contains(TEMPLATE, lib):contains(CONFIG, shared):DEFINES += QT_HTMLQT_EXPORT
    else:htmlqt-uselib:DEFINES += QT_HTMLQT_IMPORT
}
