TEMPLATE=lib

CONFIG += qt dll htmlqt-buildlib

mac:CONFIG += absolute_library_soname
win32|mac:!wince*:!win32-msvc:!macx-xcode:CONFIG += debug_and_release build_all

include(../html-qt/htmlqt.pri)

TARGET = $$HTMLQT_LIBNAME
DESTDIR = $$HTMLQT_LIBDIR

win32 {
    DLLDESTDIR = $$[QT_INSTALL_BINS]
    QMAKE_DISTCLEAN += $$[QT_INSTALL_BINS]\\$${HTMLQT_LIBNAME}.dll
}
target.path = $$DESTDIR
INSTALLS += target
