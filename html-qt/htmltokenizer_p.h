#ifndef HTMLTOKENIZER_P_H
#define HTMLTOKENIZER_P_H

#include "htmltokenizer.h"

#include <QPair>
#include <QMap>
#include <QSharedPointer>

typedef  bool (HTMLTokenizerPrivate::*HTMLTokenizerPrivateMemFn)();

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

class QT_HTMLQT_EXPORT HTMLToken
{
public:
    enum Type {
        StartTagToken,
        EndTagToken,
        CommentToken,
        DocTypeToken,
    };
    HTMLToken(Type tokenType) : type( tokenType ), selfClosing( false ), selfClosingAcknowledged( false ), forceQuirks( false )
     {}

    void appendDataCurrentAttributeName(const QChar &c)
    {
        if (data.isEmpty()) {
            data.append(qMakePair<QString,QString>(c, ""));
        } else {
            data.last().first.append(c);
        }
    }

    void appendDataCurrentAttributeValue(const QChar &c)
    {
        if (data.isEmpty()) {
            data.append(qMakePair<QString,QString>("", c));
        } else {
            data.last().second.append(c);
        }
    }

    void appendDataCurrentAttributeValue(const QString &s)
    {
        if (data.isEmpty()) {
            data.append(qMakePair<QString,QString>("", s));
        } else {
            data.last().second.append(s);
        }
    }

    QString name; // or data for comment or character types
    Type type;
    QList<QPair<QString,QString> > data;
    bool selfClosing;
    bool selfClosingAcknowledged;
    bool forceQuirks;
    QString doctypePublicId;
    QString doctypeSystemId;
    QString tagAsPure;
};

typedef QSharedPointer<HTMLToken> HTMLTokenPtr;
class HTMLTokenizerPrivate
{
    Q_DECLARE_PUBLIC(HTMLTokenizer)
public:
	HTMLTokenizerPrivate();

    // State methods
    bool dataState();
    bool characterReferenceInDataState();
    bool tagOpenState();
    bool endTagOpenState();
    bool tagNameState();
    // ... RC Raw Script
    bool beforeAttributeNameState();
    bool attributeNameState();
    bool afterAttributeNameState();
    bool beforeAttributeValueState();
    bool attributeValueDoubleQuotedState();
    bool attributeValueSingleQuotedState();
    bool attributeValueUnquotedState();
    // This method is special as for simplicity it is directly called by the callers
    void characterReferenceInAttributeValueState(QChar *additionalAllowedCharacter);
    bool afterAttributeValueQuotedState();
    bool selfClosingStartTagState();
    bool bogusCommentState();
    bool markupDeclarationOpenState();
    bool commentStartState();
    bool commentStartDashState();
    bool commentState();
    bool commentEndDashState();
    bool commentEndState();
    bool commentEndBangState();
    bool doctypeState();
    bool beforeDocTypeNameState();
    bool docTypeNameState();
    bool afterDocTypeNameState();
    bool afterDocTypePublicKeywordState();
    bool beforeDocTypePublicIdentifierState();
    bool docTypePublicIdentifierDoubleQuotedState();
    bool docTypePublicIdentifierSingleQuotedState();
    bool afterDocTypePublicIdentifierState();
    bool betweenDocTypePublicAndSystemIdentifierState();
    bool afterDocTypeSystemKeywordState();
    bool beforeDocTypeSystemIdentifierState();
    bool docTypeSystemIdentifierDoubleQuotedState();
    bool docTypeSystemIdentifierSingleQuotedState();
    bool afterDocTypeSystemIdentifierState();
    bool bogusDocTypeState();
    bool cDataSectionState();

    // auxiliary methods
    inline bool consumeStream(QChar &c)
    {
		++htmlPos;
        if (htmlPos >= htmlSize || htmlPos < 0) {
            return false;
        } else {
            c = html.at(htmlPos);
            return true;
        }
    }

    inline int streamPos() {
        return htmlPos;
    }

    inline void streamSeek(int pos) {
        htmlPos = pos;
    }

    inline void streamUnconsume(int nChars = 1) {
        htmlPos -= nChars;
    }

    //inline 
	//bool streamCanRead(int nChars = 1) {
	//       return htmlPos + nChars < htmlSize;
	//   }

    //inline 
	bool streamAtEnd() {
        return htmlPos >= htmlSize;
    }

    QString consumeEntity(QChar *allowedChar = 0);
    QChar consumeNumberEntity(bool isHex);
    void emitCurrentTagToken();

	static QMap<int, int> initReplacementCharacters();

    // current token
	HTMLTokenPtr currentToken;

    HTMLTokenizer *q_ptr;
    
	IHTMLParserInterface * parser;

    QString html;
	int tagStart;
    int htmlPos;
    int htmlSize;
    HTMLTokenizer::State state;
    HTMLTokenizerPrivateMemFn stateFn;
};

#endif // HTMLTOKENIZER_P_H

