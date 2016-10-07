#pragma once

#include <QString>
#include <QSharedPointer>

class HTMLToken;
typedef QSharedPointer<HTMLToken> HTMLTokenPtr;

#if defined(Q_OS_WIN)
#  if !defined(QT_HTMLQT_EXPORT) && !defined(QT_HTMLQT_IMPORT)
#    define QT_HTMLQT_EXPORT
#  elif defined(QT_HTMLQT_IMPORT)
#    if defined(QT_HTMLQT_EXPORT)
#      undef QT_HTMLQT_EXPORT
#    endif
#    define QT_HTMLQT_EXPORT __declspec(dllimport)
#  elif defined(QT_HTMLQT_EXPORT)
#    undef QT_HTMLQT_EXPORT
#    define QT_HTMLQT_EXPORT __declspec(dllexport)
#  endif
#else
#  define QT_HTMLQT_EXPORT
#endif

class QT_HTMLQT_EXPORT IHTMLParserInterface
{
public:
	virtual void characterToken( const QChar &c ) = 0;
	virtual void parserErrorToken( const QString &string ) = 0;
	virtual void parseToken( const HTMLTokenPtr & token ) = 0;
};

