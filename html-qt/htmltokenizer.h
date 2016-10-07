#ifndef HTMLTOKENIZER_H
#define HTMLTOKENIZER_H

#include <QObject>
#include <QSharedPointer>

class IHTMLParserInterface;
class HTMLToken;
class HTMLTokenizerPrivate;
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

class QT_HTMLQT_EXPORT HTMLTokenizer : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(HTMLTokenizer)
public:
    enum State {
        DataState,
        CharacterReferenceInDataState,
        RCDataState,
        CharacterReferenceInRCDataState,
        RawTextState,
        ScriptDataState,
        PlainTextState,
        TagOpenState,
        EndTagOpenState,
        TagNameState,
        RCDataLessThanSignState,
        RCDataEndTagOpenState,
        RCDataEndTagNameState,
        RawTextLessThanSignState,
        RawTextEndTagOpenState,
        RawTextEndTagNameState,
        ScriptDataLessThanSignState,
        ScriptDataEndTagOpenState,
        ScriptDataEndTagNameState,
        ScriptDataEscapeStartState,
        ScriptDataEscapeStartDashState,
        ScriptDataEscapedState,
        ScriptDataEscapedDashState,
        ScriptDataEscapedDashDashState,
        ScriptDataEscapedLessThanSignState,
        ScriptDataEscapedEndTagOpenState,
        ScriptDataEscapedEndTagNameState,
        ScriptDataDoubleEscapeStartState,
        ScriptDataDoubleEscapedState,
        ScriptDataDoubleEscapedDashState,
        ScriptDataDoubleEscapedDashDashState,
        ScriptDataDoubleEscapedLessThanSignState,
        ScriptDataDoubleEscapeEndState,
        BeforeAttributeNameState,
        AttributeNameState,
        AfterAttributeNameState,
        BeforeAttributeValueState,
        AttributeValueDoubleQuotedState,
        AttributeValueSingleQuotedState,
        AttributeValueUnquotedState,
        CharacterReferenceInAttributeValueState,
        AfterAttributeValueQuotedState,
        SelfClosingStartTagState,
        BogusCommentState,
        MarkupDeclarationOpenState,
        CommentStartState,
        CommentStartDashState,
        CommentState,
        CommentEndDashState,
        CommentEndState,
        CommentEndBangState,
        DocTypeState,
        BeforeDocTypeNameState,
        DocTypeNameState,
        AfterDocTypeNameState,
        AfterDocTypePublicKeywordState,
        BeforeDocTypePublicIdentifierState,
        DocTypePublicIdentifierDoubleQuotedState,
        DocTypePublicIdentifierSingleQuotedState,
        AfterDocTypePublicIdentifierState,
        BetweenDocTypePublicAndSystemIdentifierState,
        AfterDocTypeSystemKeywordState,
        BeforeDocTypeSystemIdentifierState,
        DocTypeSystemIdentifierDoubleQuotedState,
        DocTypeSystemIdentifierSingleQuotedState,
        AfterDocTypeSystemIdentifierState,
        BogusDocTypeState,
        CDataSectionState,
    };
	Q_ENUMS(State)
    HTMLTokenizer( IHTMLParserInterface & parser);
    ~HTMLTokenizer();

    void setHtmlText(const QString &html);

    State state() const;

    void start();

protected:
    void character(const QChar &c);
    void parserError(const QString &error);
    void token(const HTMLTokenPtr & token);

    HTMLTokenizerPrivate *d_ptr;
};

#endif // HTMLTOKENIZER_H
