#exists(config.pri):infile(config.pri, SOLUTIONS_LIBRARY, yes):
CONFIG += htmlqt-uselib
TEMPLATE=lib

message("common.pri")

TEMPLATE += fakelib
greaterThan(QT_MAJOR_VERSION, 5)|\
  if(equals(QT_MAJOR_VERSION, 5):greaterThan(QT_MINOR_VERSION, 4))|\
  if(equals(QT_MAJOR_VERSION, 5):equals(QT_MINOR_VERSION, 4):greaterThan(QT_PATCH_VERSION, 1)) {
    HTMLQT_LIBNAME = $$qt5LibraryTarget(htmlqt-head)
} else {
    message("TEMPLATE:")
    message($$TEMPLATE)
    message()
    message("CONFIG:")
    message($$CONFIG)
    HTMLQT_LIBNAME = $$qtLibraryTarget(htmlqt-head)
}

TEMPLATE -= fakelib

HTMLQT_LIBDIR = $$PWD/lib
unix:htmlqt-uselib:!htmlqt-buildlib:QMAKE_RPATHDIR += $$HTMLQT_LIBDIR
