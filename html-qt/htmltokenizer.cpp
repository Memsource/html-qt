

#include "htmltokenizer_p.h"
#include "htmltokenizerinterface.h"

// Qt
#include <QMetaEnum>
#include <QStringBuilder>
#include <QDebug>
#include <QPair>

#define CALL_MEMBER_FN(object,ptrToMember)  ((object).*(ptrToMember))

#define QCharTabulation 0x0009
#define QCharLineFeed 0x000a
#define QCharSpace 0x0020


#define IS_ASCII_UPPERCASE(c) ('A' <= c && c <= 'Z')
#define IS_ASCII_LOWERCASE(c) ('a' <= c && c <= 'z')
#define IS_ASCII_DIGITS(c) ('0' <= c && c <= '9')
#define IS_ASCII_HEX_DIGITS(c) (IS_ASCII_DIGITS(c) || \
    ('A' <= c && c <= 'F') || \
    ('a' <= c && c <= 'f'))
#define IS_SPACE_CHARACTER(c) (data == QCharTabulation || /* CHARACTER TABULATION (tab) */ \
    data == QCharLineFeed || /* LINE FEED (LF) */ \
    data == 0x000C || /* FORM FEED (FF) */ \
    data == QCharSpace) // SPACE

HTMLTokenizer::HTMLTokenizer( IHTMLParserInterface & parser)
	: QObject(NULL), d_ptr(new HTMLTokenizerPrivate)
{
    d_ptr->q_ptr = this;
    d_ptr->parser = &parser;

    //// TODO https://html.spec.whatwg.org/multipage/entities.json
    //// get from the url and/or keep a local copy
    //QFile entitiesFile("/home/daniel/code/html-qt/entities.json");
    //if (!entitiesFile.open(QFile::ReadOnly)) {
    //    return;
    //}
    //QJsonDocument entities = QJsonDocument::fromBinaryData(entitiesFile.readAll());
    //qDebug() << entities.object();
}

HTMLTokenizer::~HTMLTokenizer()
{
    delete d_ptr;
}

void HTMLTokenizer::setHtmlText(const QString &html)
{
    Q_D(HTMLTokenizer);
    d->html = html;
    d->htmlPos = -1;
    d->htmlSize = html.size();
}

HTMLTokenizer::State HTMLTokenizer::state() const
{
    Q_D(const HTMLTokenizer);
    return d->state;
}

void HTMLTokenizer::start()
{
    Q_D(HTMLTokenizer);

    int lastPos = d->streamPos();
    int repeatedPos = 0;
    while (CALL_MEMBER_FN(*d, d->stateFn)() && !d->streamAtEnd()) {
        // dunno what to do here :)
//        qDebug() << d->streamPos() << metaObject()->enumerator(0).key(d->state) << d->streamAtEnd();
        if (lastPos == d->streamPos()) {
            if (++repeatedPos > 10) {
				return;
                //qFatal("Infinite loop detected on state: %s, at position: %d",
                //       metaObject()->enumerator(0).key(d->state),
                //       lastPos);
            }
        } else {
            lastPos = d->streamPos();
            repeatedPos = 0;
        }
    }
    //qDebug() << "finished";
}

void HTMLTokenizer::character(const QChar &c)
{
    Q_D(HTMLTokenizer);
    d->parser->characterToken(c);
}

void HTMLTokenizer::parserError(const QString &error)
{
    Q_D(HTMLTokenizer);
    d->parser->parserErrorToken(error);
}

void HTMLTokenizer::token(const HTMLTokenPtr & token)
{
    Q_D(HTMLTokenizer);
    d->parser->parseToken(token);
}


HTMLTokenizerPrivate::HTMLTokenizerPrivate()
	: htmlPos( -1 ), htmlSize( 0 ), state( HTMLTokenizer::DataState ), stateFn( &HTMLTokenizerPrivate::dataState )
{
};

// https://html.spec.whatwg.org/multipage/syntax.html#data-state
bool HTMLTokenizerPrivate::dataState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        // Tokenization ends.
        return false;
    } else if (data == '&') {
        state = HTMLTokenizer::CharacterReferenceInDataState;
        stateFn = &HTMLTokenizerPrivate::characterReferenceInDataState;
    } else if (data == '<') {
		tagStart = htmlPos;
        state = HTMLTokenizer::TagOpenState;
        stateFn = &HTMLTokenizerPrivate::tagOpenState;
    } else if (data.isNull()) {
        state = HTMLTokenizer::TagOpenState;
        Q_EMIT q->parserError("invalid-codepoint");
        Q_EMIT q->character(data);
    } else {
        Q_EMIT q->character(data);
    }

    return true;
}

// https://html.spec.whatwg.org/multipage/syntax.html#character-reference-in-data-state
bool HTMLTokenizerPrivate::characterReferenceInDataState()
{
    Q_Q(HTMLTokenizer);

    const QString &ret = consumeEntity();
    if (ret.isNull()) {
        q->character('&');
    } else {
        QString::ConstIterator it = ret.constBegin();
        while (it != ret.constEnd()) {
            q->character(*it);
            ++it;
        }
    }
    state = HTMLTokenizer::DataState;
    stateFn = &HTMLTokenizerPrivate::dataState;
    return true;
}

// https://html.spec.whatwg.org/multipage/syntax.html#tag-open-state
bool HTMLTokenizerPrivate::tagOpenState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("expected-tag-name"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        Q_EMIT q->character('<');
        streamUnconsume();
    } else if (data == '!') {
        state = HTMLTokenizer::MarkupDeclarationOpenState;
        stateFn = &HTMLTokenizerPrivate::markupDeclarationOpenState;
    } else if (data == '/') {
        state = HTMLTokenizer::EndTagOpenState;
        stateFn = &HTMLTokenizerPrivate::endTagOpenState;
    } else if (IS_ASCII_UPPERCASE(data)) {
        state = HTMLTokenizer::TagNameState;
        stateFn = &HTMLTokenizerPrivate::tagNameState;
        currentToken = HTMLTokenPtr( new HTMLToken( HTMLToken::StartTagToken ) );
        currentToken->name = data.toLower();
    } else if (IS_ASCII_LOWERCASE(data)) {
        state = HTMLTokenizer::TagNameState;
        stateFn = &HTMLTokenizerPrivate::tagNameState;
        currentToken = HTMLTokenPtr( new HTMLToken(HTMLToken::StartTagToken));
        currentToken->name = data;
    } else if (data == '?') {
        q->parserError( QString("expected-tag-name-but-got-question-mark"));
        state = HTMLTokenizer::BogusCommentState;
        stateFn = &HTMLTokenizerPrivate::bogusCommentState;
    } else {
        q->parserError( QString("expected-tag-name"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        q->character('<');
        streamUnconsume();
    }

    return true;
}

// https://html.spec.whatwg.org/multipage/syntax.html#end-tag-open-state
bool HTMLTokenizerPrivate::endTagOpenState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError( QString("expected-closing-tag-but-got-eof"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        Q_EMIT q->character('<'); // 0x003C
        Q_EMIT q->character('/'); // 0x002F
        streamUnconsume();
    } else if (IS_ASCII_UPPERCASE(data)) {
        currentToken = HTMLTokenPtr( new HTMLToken(HTMLToken::EndTagToken));
        currentToken->name = data.toLower();
        currentToken->selfClosing = false;
        state = HTMLTokenizer::TagNameState;
        stateFn = &HTMLTokenizerPrivate::tagNameState;
    } else if (IS_ASCII_LOWERCASE(data)) {
        currentToken = HTMLTokenPtr( new HTMLToken(HTMLToken::EndTagToken));
        currentToken->name = data;
        currentToken->selfClosing = false;
        state = HTMLTokenizer::TagNameState;
        stateFn = &HTMLTokenizerPrivate::tagNameState;
    } else if (data == '>') {
        Q_EMIT q->parserError( QString("expected-closing-tag-but-got-right-bracket"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
    } else {
        Q_EMIT q->parserError( QString("expected-closing-tag-but-got-char"));
        state = HTMLTokenizer::BogusCommentState;
        stateFn = &HTMLTokenizerPrivate::bogusCommentState;
    }

    return true;
}

bool HTMLTokenizerPrivate::tagNameState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError( QString("eof-in-tag-name"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        streamUnconsume();
    } else if (IS_SPACE_CHARACTER(data)) {
        state = HTMLTokenizer::BeforeAttributeNameState;
        stateFn = &HTMLTokenizerPrivate::beforeAttributeNameState;
    } else if (data == '/') {
        state = HTMLTokenizer::SelfClosingStartTagState;
        stateFn = &HTMLTokenizerPrivate::selfClosingStartTagState;
    } else if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;

		//qDebug() << html.midRef( tagStart, htmlPos - tagStart );

        emitCurrentTagToken();
    } else if (IS_ASCII_UPPERCASE(data)) {
        // Appending the lower case version
        currentToken->name.append(data.toLower());
    } else if (data.isNull()) {
        Q_EMIT q->parserError( QString("invalid-codepoint"));
        currentToken->name.append(QChar::ReplacementCharacter);
    } else {
        currentToken->name.append(data);
    }

    return true;
}

bool HTMLTokenizerPrivate::beforeAttributeNameState()
{
    Q_Q(HTMLTokenizer);

    QChar data;
    do {
        if (!consumeStream(data)) {
            Q_EMIT q->parserError( QString("expected-attribute-name-but-got-eof"));
            state = HTMLTokenizer::DataState;
            stateFn = &HTMLTokenizerPrivate::dataState;
            streamUnconsume();
            return true;
        }
    } while (IS_SPACE_CHARACTER(data)); // Ignore all space characters

    if (data == '/') {
        state = HTMLTokenizer::SelfClosingStartTagState;
        stateFn = &HTMLTokenizerPrivate::selfClosingStartTagState;
    } else if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;

		//qDebug() << html.midRef( tagStart, htmlPos - tagStart );

        emitCurrentTagToken();
    } else if (IS_ASCII_UPPERCASE(data)) {
        // Appending the lower case version
		currentToken->data.append( QPair<QString, QString>( data.toLower(), QString() ) );
        state = HTMLTokenizer::AttributeNameState;
        stateFn = &HTMLTokenizerPrivate::attributeNameState;
    } else if (data.isNull()) {
        Q_EMIT q->parserError( QString("invalid-codepoint"));
        currentToken->data.append( QPair<QString, QString>((QChar)QChar::ReplacementCharacter, QString()));
        state = HTMLTokenizer::AttributeNameState;
        stateFn = &HTMLTokenizerPrivate::attributeNameState;
    } else if (data == '"' ||
               data == '\'' ||
               data == '<' ||
               data == '=') {
        Q_EMIT q->parserError( QString("invalid-character-in-attribute-name"));
        currentToken->data.append( QPair<QString, QString>(data, QString()));
        state = HTMLTokenizer::AttributeNameState;
        stateFn = &HTMLTokenizerPrivate::attributeNameState;
    } else {
        currentToken->data.append( QPair<QString, QString>(data, QString()));
        state = HTMLTokenizer::AttributeNameState;
        stateFn = &HTMLTokenizerPrivate::attributeNameState;
    }

    return true;
}

bool HTMLTokenizerPrivate::attributeNameState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError( QString("eof-in-attribute-name"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        streamUnconsume();
    } else if (IS_SPACE_CHARACTER(data)) {
        state = HTMLTokenizer::AfterAttributeNameState;
        stateFn = &HTMLTokenizerPrivate::afterAttributeNameState;
    } else if (data == '/') {
        state = HTMLTokenizer::SelfClosingStartTagState;
        stateFn = &HTMLTokenizerPrivate::selfClosingStartTagState;
    } else if (data == '=') {
        state = HTMLTokenizer::BeforeAttributeValueState;
        stateFn = &HTMLTokenizerPrivate::beforeAttributeValueState;
    } else if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;

		//qDebug() << html.midRef( tagStart, htmlPos - tagStart );

        emitCurrentTagToken();
    } else if (IS_ASCII_UPPERCASE(data)) {
        currentToken->appendDataCurrentAttributeName(data.toLower());
    } else if (data.isNull()) {
        Q_EMIT q->parserError( QString("invalid-codepoint"));
        currentToken->appendDataCurrentAttributeName(QChar::ReplacementCharacter);
    } else if (data == '"' || data == '\'' || data == '<') {
        Q_EMIT q->parserError( QString("invalid-character-in-attribute-name"));
        currentToken->appendDataCurrentAttributeName(data);
    } else {
        currentToken->appendDataCurrentAttributeName(data);
    }

    return true;
}

bool HTMLTokenizerPrivate::afterAttributeNameState()
{
    Q_Q(HTMLTokenizer);

    QChar data;
    do {
        if (!consumeStream(data)) {
            Q_EMIT q->parserError( QString("expected-end-of-tag-but-got-eof"));
            state = HTMLTokenizer::DataState;
            stateFn = &HTMLTokenizerPrivate::dataState;
            streamUnconsume();
            return true;
        }
    } while (IS_SPACE_CHARACTER(data)); // Ignore all space characters

    if (data == '/') {
        state = HTMLTokenizer::SelfClosingStartTagState;
        stateFn = &HTMLTokenizerPrivate::selfClosingStartTagState;
    } else if (data == '=') {
        state = HTMLTokenizer::BeforeAttributeValueState;
        stateFn = &HTMLTokenizerPrivate::beforeAttributeValueState;
    } else if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;

		//qDebug() << html.midRef( tagStart, htmlPos - tagStart );

        emitCurrentTagToken();
    } else if (IS_ASCII_UPPERCASE(data)) {
        currentToken->data.append(qMakePair<QString,QString>(data.toLower(), QString()));
        state = HTMLTokenizer::AttributeNameState;
        stateFn = &HTMLTokenizerPrivate::attributeNameState;
    } else if (data.isNull()) {
        Q_EMIT q->parserError( QString("invalid-codepoint"));
        currentToken->data.append(qMakePair<QString,QString>(QChar(QChar::ReplacementCharacter), QString()));
        state = HTMLTokenizer::AttributeNameState;
        stateFn = &HTMLTokenizerPrivate::attributeNameState;
    } else if (data == '"' || data == '\'' || data == '<') {
        Q_EMIT q->parserError( QString("invalid-character-after-attribute-name"));
        currentToken->data.append(qMakePair<QString,QString>(data, QString()));
        state = HTMLTokenizer::AttributeNameState;
        stateFn = &HTMLTokenizerPrivate::attributeNameState;
    } else {
        currentToken->data.append(qMakePair<QString,QString>(data, QString()));
        state = HTMLTokenizer::AttributeNameState;
        stateFn = &HTMLTokenizerPrivate::attributeNameState;
    }

    return true;
}

bool HTMLTokenizerPrivate::beforeAttributeValueState()
{
    Q_Q(HTMLTokenizer);

    QChar data;
    do {
        if (!consumeStream(data)) {
            Q_EMIT q->parserError( QString("expected-attribute-value-but-got-eof"));
            state = HTMLTokenizer::DataState;
            stateFn = &HTMLTokenizerPrivate::dataState;
            streamUnconsume();
            return true;
        }
    } while (IS_SPACE_CHARACTER(data)); // Ignore all space characters

    if (data == '"') {
        state = HTMLTokenizer::AttributeValueDoubleQuotedState;
        stateFn = &HTMLTokenizerPrivate::attributeValueDoubleQuotedState;
    } else if (data == '&') {
        state = HTMLTokenizer::AttributeValueUnquotedState;
        stateFn = &HTMLTokenizerPrivate::attributeValueUnquotedState;
    } else if (data == '\'') {
        state = HTMLTokenizer::AttributeValueSingleQuotedState;
        stateFn = &HTMLTokenizerPrivate::attributeValueSingleQuotedState;
    } else if (data.isNull()) {
        Q_EMIT q->parserError( QString("expected-attribute-value-but-got-right-bracket"));
        emitCurrentTagToken();
    } else if (data == '>') {
        Q_EMIT q->parserError( QString("expected-attribute-value-but-got-right-bracket"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else if (data == '<' || data == '=' || data == '`') {
        Q_EMIT q->parserError( QString("equals-in-unquoted-attribute-value"));
        currentToken->appendDataCurrentAttributeValue(data);
        state = HTMLTokenizer::AttributeValueUnquotedState;
        stateFn = &HTMLTokenizerPrivate::attributeValueUnquotedState;
    } else {
        currentToken->appendDataCurrentAttributeValue(data);
        state = HTMLTokenizer::AttributeValueUnquotedState;
        stateFn = &HTMLTokenizerPrivate::attributeValueUnquotedState;
    }

    return true;
}

bool HTMLTokenizerPrivate::attributeValueDoubleQuotedState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-attribute-value-double-quote"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        streamUnconsume();
    } else if (data == '"') {
        state = HTMLTokenizer::AfterAttributeValueQuotedState;
        stateFn = &HTMLTokenizerPrivate::afterAttributeValueQuotedState;
    } else if (data == '&') {
        QChar allowedChar('"');
        characterReferenceInAttributeValueState(&allowedChar);
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        currentToken->appendDataCurrentAttributeValue(QChar::ReplacementCharacter);
    } else {
        currentToken->appendDataCurrentAttributeValue(data);
    }

    return true;
}

bool HTMLTokenizerPrivate::attributeValueSingleQuotedState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-attribute-value-single-quote"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        streamUnconsume();
    } else if (data == '\'') {
        state = HTMLTokenizer::AfterAttributeValueQuotedState;
        stateFn = &HTMLTokenizerPrivate::afterAttributeValueQuotedState;
    } else if (data == '&') {
        QChar allowedChar('\'');
        characterReferenceInAttributeValueState(&allowedChar);
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        currentToken->appendDataCurrentAttributeValue(QChar::ReplacementCharacter);
    } else {
        currentToken->appendDataCurrentAttributeValue(data);
    }

    return true;
}

bool HTMLTokenizerPrivate::attributeValueUnquotedState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-attribute-value-no-quotes"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        streamUnconsume();
    } else if (IS_SPACE_CHARACTER(data)) {
        state = HTMLTokenizer::BeforeAttributeNameState;
        stateFn = &HTMLTokenizerPrivate::beforeAttributeNameState;
    } else if (data == '&') {
        QChar allowedChar('>');
        characterReferenceInAttributeValueState(&allowedChar);
    } else if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;

		//qDebug() << html.midRef( tagStart, htmlPos - tagStart );

        emitCurrentTagToken();
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        currentToken->appendDataCurrentAttributeValue(QChar::ReplacementCharacter);
    } else if (data == '"' || data == '\'' || data == '<' || data == '`') {
        Q_EMIT q->parserError(QString("unexpected-character-in-unquoted-attribute-value"));
        currentToken->appendDataCurrentAttributeValue(data);
    } else {
        currentToken->appendDataCurrentAttributeValue(data);
    }

    return true;
}

void HTMLTokenizerPrivate::characterReferenceInAttributeValueState(QChar *additionalAllowedCharacter)
{
    QString ret = consumeEntity(additionalAllowedCharacter);
    if (ret.isNull()) {
        currentToken->appendDataCurrentAttributeValue('&');
    } else {
        currentToken->appendDataCurrentAttributeValue(ret);
    }
}

bool HTMLTokenizerPrivate::afterAttributeValueQuotedState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("unexpected-eof-after-attribute-value"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        streamUnconsume();
    } else if (IS_SPACE_CHARACTER(data)) {
        state = HTMLTokenizer::BeforeAttributeNameState;
        stateFn = &HTMLTokenizerPrivate::beforeAttributeNameState;
    } else if (data == '/') {
        state = HTMLTokenizer::SelfClosingStartTagState;
        stateFn = &HTMLTokenizerPrivate::selfClosingStartTagState;
    } else if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;

		//qDebug() << html.midRef( tagStart, htmlPos - tagStart );

        emitCurrentTagToken();
    } else {
        Q_EMIT q->parserError(QString("unexpected-character-after-attribute-value"));
        state = HTMLTokenizer::BeforeAttributeNameState;
        stateFn = &HTMLTokenizerPrivate::beforeAttributeNameState;
        streamUnconsume();
    }

    return true;
}

bool HTMLTokenizerPrivate::selfClosingStartTagState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("unexpected-eof-after-solidus-in-tag"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        streamUnconsume();
    } else if (data == '>') {
        currentToken->selfClosing = true;
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;

		//qDebug() << html.midRef( tagStart, htmlPos - tagStart );

        emitCurrentTagToken();
    } else {
        Q_EMIT q->parserError(QString("unexpected-character-after-solidus-in-tag"));
        state = HTMLTokenizer::BeforeAttributeNameState;
        stateFn = &HTMLTokenizerPrivate::beforeAttributeNameState;
        streamUnconsume();
    }

    return true;
}

bool HTMLTokenizerPrivate::bogusCommentState()
{
    // TODO
    return true;
}

// https://html.spec.whatwg.org/multipage/syntax.html#markup-declaration-open-state
bool HTMLTokenizerPrivate::markupDeclarationOpenState()
{
    Q_Q(HTMLTokenizer);

    int initalPos = streamPos();
    QChar data;
    // TODO check this
    consumeStream(data);
    QString charStack = data;

    if (data == '-') {
        // TODO check this
        consumeStream(data);
        charStack.append(data);
        if (data == '-') {
            currentToken = HTMLTokenPtr( new HTMLToken(HTMLToken::CommentToken));
            currentToken->name = "";
            state = HTMLTokenizer::CommentStartState;
            stateFn = &HTMLTokenizerPrivate::commentStartState;
            return true;
        }
    } else if (data == 'd' || data == 'D') {
        // consume more 6 chars
        for (int i = 0; i < 6; ++i) {
            // TODO check this
            consumeStream(data);
            charStack.append(data);
        }

        if (charStack.compare(QLatin1String("DOCTYPE"), Qt::CaseInsensitive) == 0) {
//            currentToken = new HTMLToken(HTMLToken::CommentToken);
            qDebug() << "markupDeclarationOpenState" << charStack;
            state = HTMLTokenizer::DocTypeState;
            stateFn = &HTMLTokenizerPrivate::doctypeState;
            return true;
        }
    } else if (data == '[') {
        qWarning() << "markupDeclarationOpenState CDATA TODO";
    }

    Q_EMIT q->parserError(QString("expected-dashes-or-doctype"));
    state = HTMLTokenizer::BogusCommentState;
    stateFn = &HTMLTokenizerPrivate::bogusCommentState;
    streamSeek(initalPos);

    return true;
}

bool HTMLTokenizerPrivate::commentStartState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-comment"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (data == '-') {
        state = HTMLTokenizer::CommentStartDashState;
        stateFn = &HTMLTokenizerPrivate::commentStartDashState;
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        currentToken->name.append(QChar::ReplacementCharacter);
        state = HTMLTokenizer::CommentState;
        stateFn = &HTMLTokenizerPrivate::commentState;
    } else if (data == '>') {
        Q_EMIT q->parserError(QString("incorrect-comment"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else {
        currentToken->name.append(data);
        state = HTMLTokenizer::CommentState;
        stateFn = &HTMLTokenizerPrivate::commentState;
    }

    return true;
}

bool HTMLTokenizerPrivate::commentStartDashState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-comment"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (data == '-') {
        state = HTMLTokenizer::CommentEndState;
        stateFn = &HTMLTokenizerPrivate::commentEndState;
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        // TODO see if we can reduce to a singe call
        currentToken->name.append('-');
        currentToken->name.append(QChar::ReplacementCharacter);
        state = HTMLTokenizer::CommentState;
        stateFn = &HTMLTokenizerPrivate::commentState;
    } else if (data == '>') {
        Q_EMIT q->parserError(QString("incorrect-comment"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else {
        // TODO see if we can reduce to a singe call
        currentToken->name.append('-');
        currentToken->name.append(data);
        state = HTMLTokenizer::CommentState;
        stateFn = &HTMLTokenizerPrivate::commentState;
    }

    return true;
}

bool HTMLTokenizerPrivate::commentState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-comment"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (data == '-') {
        state = HTMLTokenizer::CommentEndDashState;
        stateFn = &HTMLTokenizerPrivate::commentEndDashState;
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        currentToken->name.append(QChar::ReplacementCharacter);
    } else {
        currentToken->name.append(data);
    }

    return true;
}

bool HTMLTokenizerPrivate::commentEndDashState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-comment-end-dash"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (data == '-') {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        // TODO see if we can reduce to a singe call
        currentToken->name.append('-');
        currentToken->name.append(QChar::ReplacementCharacter);
        state = HTMLTokenizer::CommentEndState;
        stateFn = &HTMLTokenizerPrivate::commentEndState;
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        currentToken->name.append(QChar::ReplacementCharacter);
        state = HTMLTokenizer::CommentState;
        stateFn = &HTMLTokenizerPrivate::commentState;
    } else {
        currentToken->name.append('-');
        currentToken->name.append(data);
        state = HTMLTokenizer::CommentState;
        stateFn = &HTMLTokenizerPrivate::commentState;
    }

    return true;
}

bool HTMLTokenizerPrivate::commentEndState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-comment-double-dash"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        // TODO see if we can reduce to a singe call
        currentToken->name.append('-');
        currentToken->name.append(QChar::ReplacementCharacter);
        state = HTMLTokenizer::CommentState;
        stateFn = &HTMLTokenizerPrivate::commentState;
    } else if (data == '!') {
        Q_EMIT q->parserError(QString("unexpected-bang-after-double-dash-in-comment"));
        state = HTMLTokenizer::CommentEndBangState;
        stateFn = &HTMLTokenizerPrivate::commentEndBangState;
    } else if (data == '-') {
        Q_EMIT q->parserError(QString("unexpected-dash-after-double-dash-in-comment"));
        currentToken->name.append('-');
    } else {
        Q_EMIT q->parserError(QString("unexpected-char-in-comment"));
        currentToken->name.append(QLatin1String("--") % data);
        state = HTMLTokenizer::CommentState;
        stateFn = &HTMLTokenizerPrivate::commentState;
    }

    return true;
}

bool HTMLTokenizerPrivate::commentEndBangState()
{
    // TODO
    return true;
}

// https://html.spec.whatwg.org/multipage/syntax.html#doctype-state
bool HTMLTokenizerPrivate::doctypeState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("expected-doctype-name-but-got-eof"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        currentToken = HTMLTokenPtr( new HTMLToken(HTMLToken::DocTypeToken));
        currentToken->forceQuirks = true;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (IS_SPACE_CHARACTER(data)) {
        state = HTMLTokenizer::BeforeDocTypeNameState;
        stateFn = &HTMLTokenizerPrivate::beforeDocTypeNameState;
    } else {
        Q_EMIT q->parserError(QString("need-space-after-doctype"));
        state = HTMLTokenizer::BeforeDocTypeNameState;
        stateFn = &HTMLTokenizerPrivate::beforeDocTypeNameState;
        streamUnconsume();
    }

    return true;
}

bool HTMLTokenizerPrivate::beforeDocTypeNameState()
{
    Q_Q(HTMLTokenizer);

    QChar data;
    do {
        if (!consumeStream(data)) {
            Q_EMIT q->parserError(QString("expected-doctype-name-but-got-eof"));
            state = HTMLTokenizer::DataState;
            stateFn = &HTMLTokenizerPrivate::dataState;
            currentToken = HTMLTokenPtr( new HTMLToken(HTMLToken::DocTypeToken) );
            currentToken->forceQuirks = true;
            emitCurrentTagToken();
            streamUnconsume();
            return true;
        }
    } while (IS_SPACE_CHARACTER(data)); // Ignore all space characters

    if (IS_ASCII_UPPERCASE(data)) {
        currentToken = HTMLTokenPtr( new HTMLToken(HTMLToken::DocTypeToken) );
        currentToken->name = data.toLower();
        state = HTMLTokenizer::DocTypeNameState;
        stateFn = &HTMLTokenizerPrivate::docTypeNameState;
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        currentToken = HTMLTokenPtr( new HTMLToken(HTMLToken::DocTypeToken) );
        currentToken->name = QChar(QChar::ReplacementCharacter);
        state = HTMLTokenizer::DocTypeNameState;
        stateFn = &HTMLTokenizerPrivate::docTypeNameState;
    } else if (data == '>') {
        Q_EMIT q->parserError(QString("expected-doctype-name-but-got-right-bracket"  ));
        currentToken = HTMLTokenPtr( new HTMLToken(HTMLToken::DocTypeToken) );
        currentToken->forceQuirks = true;
        emitCurrentTagToken();
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
    } else {
        currentToken = HTMLTokenPtr( new HTMLToken(HTMLToken::DocTypeToken));
        currentToken->name = data;
        state = HTMLTokenizer::DocTypeNameState;
        stateFn = &HTMLTokenizerPrivate::docTypeNameState;
    }

    return true;
}

bool HTMLTokenizerPrivate::docTypeNameState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-doctype-name"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        currentToken = HTMLTokenPtr( new HTMLToken(HTMLToken::DocTypeToken));
        currentToken->forceQuirks = true;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (IS_SPACE_CHARACTER(data)) {
        state = HTMLTokenizer::AfterDocTypeNameState;
        stateFn = &HTMLTokenizerPrivate::afterDocTypeNameState;
    } else if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else if (IS_ASCII_UPPERCASE(data)) {
        currentToken->name.append(data.toLower());
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        currentToken->name.append(QChar::ReplacementCharacter);
    } else {
        currentToken->name.append(data);
    }

    return true;
}

// https://html.spec.whatwg.org/multipage/syntax.html#after-doctype-name-state
bool HTMLTokenizerPrivate::afterDocTypeNameState()
{
    Q_Q(HTMLTokenizer);

    QChar data;
    do {
        if (!consumeStream(data)) {
            Q_EMIT q->parserError(QString("eof-in-doctype"));
            state = HTMLTokenizer::DataState;
            stateFn = &HTMLTokenizerPrivate::dataState;
            currentToken->forceQuirks = true;
            emitCurrentTagToken();
            streamUnconsume();
            return true;
        }
    } while (IS_SPACE_CHARACTER(data)); // Ignore all space characters

    if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else {
        int initalPos = streamPos();
        if (data == 'p' || data == 'P' ||
                data == 's' || data == 'S') {
            QString charStack = data;
            // consume more 5 chars
            for (int i = 0; i < 5; ++i) {
                // TODO check this
                consumeStream(data);
                charStack.append(data);
            }

            if (charStack.compare(QLatin1String("PUBLIC"), Qt::CaseInsensitive) == 0) {
                state = HTMLTokenizer::AfterDocTypePublicKeywordState;
                stateFn = &HTMLTokenizerPrivate::afterDocTypePublicKeywordState;
                return true;
            } else if (charStack.compare(QLatin1String("SYSTEM"), Qt::CaseInsensitive) == 0) {
                state = HTMLTokenizer::AfterDocTypeSystemKeywordState;
                stateFn = &HTMLTokenizerPrivate::afterDocTypeSystemKeywordState;
                return true;
            }
        }

        Q_EMIT q->parserError(QString("expected-space-or-right-bracket-in-doctype"));
        state = HTMLTokenizer::BogusDocTypeState;
        stateFn = &HTMLTokenizerPrivate::bogusDocTypeState;
        currentToken->forceQuirks = true;
        streamSeek(initalPos);
    }

    return true;
}

bool HTMLTokenizerPrivate::afterDocTypePublicKeywordState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-doctype"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        currentToken->forceQuirks = true;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (IS_SPACE_CHARACTER(data)) {
        state = HTMLTokenizer::BeforeDocTypePublicIdentifierState;
        stateFn = &HTMLTokenizerPrivate::beforeDocTypePublicIdentifierState;
    } else if (data == '"') {
        Q_EMIT q->parserError(QString("unexpected-double-quote-in-doctype"));
        currentToken->doctypePublicId = "";
        state = HTMLTokenizer::DocTypePublicIdentifierDoubleQuotedState;
        stateFn = &HTMLTokenizerPrivate::docTypePublicIdentifierDoubleQuotedState;
    } else if (data == '\'') {
        Q_EMIT q->parserError(QString("unexpected-single-quote-in-doctype"));
        currentToken->doctypePublicId = "";
        state = HTMLTokenizer::DocTypePublicIdentifierSingleQuotedState;
        stateFn = &HTMLTokenizerPrivate::docTypePublicIdentifierSingleQuotedState;
    } else if (data == '>') {
        Q_EMIT q->parserError(QString("unexpected-single-quote-in-doctype"));
        currentToken->forceQuirks = true;
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else {
        Q_EMIT q->parserError(QString("unexpected-char-in-doctype"));
        currentToken->forceQuirks = true;
        emitCurrentTagToken();
        state = HTMLTokenizer::BogusDocTypeState;
        stateFn = &HTMLTokenizerPrivate::bogusDocTypeState;
    }

    return true;
}

bool HTMLTokenizerPrivate::beforeDocTypePublicIdentifierState()
{
    Q_Q(HTMLTokenizer);

    QChar data;
    do {
        if (!consumeStream(data)) {
            Q_EMIT q->parserError(QString("eof-in-doctype"));
            state = HTMLTokenizer::DataState;
            stateFn = &HTMLTokenizerPrivate::dataState;
            currentToken->forceQuirks = true;
            emitCurrentTagToken();
            streamUnconsume();
            return true;
        }
    } while (IS_SPACE_CHARACTER(data)); // Ignore all space characters

    if (data == '"') {
        currentToken->doctypePublicId = "";
        state = HTMLTokenizer::DocTypePublicIdentifierDoubleQuotedState;
        stateFn = &HTMLTokenizerPrivate::docTypePublicIdentifierDoubleQuotedState;
    } else if (data == '\'') {
        currentToken->doctypePublicId = "";
        state = HTMLTokenizer::DocTypePublicIdentifierSingleQuotedState;
        stateFn = &HTMLTokenizerPrivate::docTypePublicIdentifierSingleQuotedState;
    } else if (data == '>') {
        Q_EMIT q->parserError(QString("unexpected-end-of-doctype"));
        currentToken->forceQuirks = true;
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else {
        Q_EMIT q->parserError(QString("unexpected-char-in-doctype"));
        currentToken->forceQuirks = true;
        emitCurrentTagToken();
        state = HTMLTokenizer::BogusDocTypeState;
        stateFn = &HTMLTokenizerPrivate::bogusDocTypeState;
    }

    return true;
}

bool HTMLTokenizerPrivate::docTypePublicIdentifierDoubleQuotedState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-doctype"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        currentToken->forceQuirks = true;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (data == '"') {
        state = HTMLTokenizer::AfterDocTypePublicIdentifierState;
        stateFn = &HTMLTokenizerPrivate::afterDocTypePublicIdentifierState;
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        currentToken->name.append(QChar::ReplacementCharacter);
    } else if (data == '>') {
        Q_EMIT q->parserError(QString("unexpected-end-of-doctype"));
        currentToken->forceQuirks = true;
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else {
        currentToken->doctypePublicId.append(data);
    }

    return true;
}

bool HTMLTokenizerPrivate::docTypePublicIdentifierSingleQuotedState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-doctype"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        currentToken->forceQuirks = true;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (data == '\'') {
        state = HTMLTokenizer::AfterDocTypePublicIdentifierState;
        stateFn = &HTMLTokenizerPrivate::afterDocTypePublicIdentifierState;
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        currentToken->name.append(QChar::ReplacementCharacter);
    } else if (data == '>') {
        Q_EMIT q->parserError(QString("unexpected-end-of-doctype"));
        currentToken->forceQuirks = true;
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else {
        currentToken->doctypePublicId.append(data);
    }

    return true;
}

bool HTMLTokenizerPrivate::afterDocTypePublicIdentifierState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-doctype"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        currentToken->forceQuirks = true;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (IS_SPACE_CHARACTER(data)) {
        state = HTMLTokenizer::BetweenDocTypePublicAndSystemIdentifierState;
        stateFn = &HTMLTokenizerPrivate::betweenDocTypePublicAndSystemIdentifierState;
    } else if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else if (data == '"') {
        Q_EMIT q->parserError(QString("unexpected-char-in-doctype"));
        currentToken->doctypeSystemId = "";
        state = HTMLTokenizer::DocTypeSystemIdentifierDoubleQuotedState;
        stateFn = &HTMLTokenizerPrivate::docTypeSystemIdentifierDoubleQuotedState;
    } else if (data == '\'') {
        Q_EMIT q->parserError(QString("unexpected-char-in-doctype"));
        currentToken->doctypeSystemId = "";
        state = HTMLTokenizer::DocTypeSystemIdentifierSingleQuotedState;
        stateFn = &HTMLTokenizerPrivate::docTypeSystemIdentifierSingleQuotedState;
    } else {
        q->parserError(QString("unexpected-char-in-doctype"));
        currentToken->forceQuirks = true;
        state = HTMLTokenizer::BogusDocTypeState;
        stateFn = &HTMLTokenizerPrivate::bogusDocTypeState;
    }

    return true;
}

bool HTMLTokenizerPrivate::betweenDocTypePublicAndSystemIdentifierState()
{
    Q_Q(HTMLTokenizer);

    QChar data;
    do {
        if (!consumeStream(data)) {
            Q_EMIT q->parserError(QString("eof-in-doctype"));
            state = HTMLTokenizer::DataState;
            stateFn = &HTMLTokenizerPrivate::dataState;
            currentToken->forceQuirks = true;
            emitCurrentTagToken();
            streamUnconsume();
            return true;
        }
    } while (IS_SPACE_CHARACTER(data)); // Ignore all space characters

    if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else if (data == '"') {
        currentToken->doctypeSystemId = "";
        state = HTMLTokenizer::DocTypeSystemIdentifierDoubleQuotedState;
        stateFn = &HTMLTokenizerPrivate::docTypeSystemIdentifierDoubleQuotedState;
    } else if (data == '\'') {
        currentToken->doctypeSystemId = "";
        state = HTMLTokenizer::DocTypeSystemIdentifierSingleQuotedState;
        stateFn = &HTMLTokenizerPrivate::docTypeSystemIdentifierSingleQuotedState;
    } else {
        q->parserError(QString("unexpected-char-in-doctype"));
        currentToken->forceQuirks = true;
        state = HTMLTokenizer::BogusDocTypeState;
        stateFn = &HTMLTokenizerPrivate::bogusDocTypeState;
    }

    return true;
}

bool HTMLTokenizerPrivate::afterDocTypeSystemKeywordState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-doctype"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        currentToken->forceQuirks = true;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (IS_SPACE_CHARACTER(data)) {
        state = HTMLTokenizer::BeforeDocTypeSystemIdentifierState;
        stateFn = &HTMLTokenizerPrivate::beforeDocTypeSystemIdentifierState;
    } else if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else if (data == '"') {
        Q_EMIT q->parserError(QString("unexpected-char-in-doctype"));
        currentToken->doctypeSystemId = "";
        state = HTMLTokenizer::DocTypeSystemIdentifierDoubleQuotedState;
        stateFn = &HTMLTokenizerPrivate::docTypeSystemIdentifierDoubleQuotedState;
    } else if (data == '\'') {
        Q_EMIT q->parserError(QString("unexpected-char-in-doctype"));
        currentToken->doctypeSystemId = "";
        state = HTMLTokenizer::DocTypeSystemIdentifierSingleQuotedState;
        stateFn = &HTMLTokenizerPrivate::docTypeSystemIdentifierSingleQuotedState;
    } else {
        q->parserError(QString("unexpected-char-in-doctype"));
        currentToken->forceQuirks = true;
        state = HTMLTokenizer::BogusDocTypeState;
        stateFn = &HTMLTokenizerPrivate::bogusDocTypeState;
    }

    return true;
}

bool HTMLTokenizerPrivate::beforeDocTypeSystemIdentifierState()
{
    Q_Q(HTMLTokenizer);

    QChar data;
    do {
        if (!consumeStream(data)) {
            Q_EMIT q->parserError(QString("eof-in-doctype"));
            state = HTMLTokenizer::DataState;
            stateFn = &HTMLTokenizerPrivate::dataState;
            currentToken->forceQuirks = true;
            emitCurrentTagToken();
            streamUnconsume();
            return true;
        }
    } while (IS_SPACE_CHARACTER(data)); // Ignore all space characters

    if (data == '"') {
        currentToken->doctypeSystemId = "";
        state = HTMLTokenizer::DocTypeSystemIdentifierDoubleQuotedState;
        stateFn = &HTMLTokenizerPrivate::docTypeSystemIdentifierDoubleQuotedState;
    } else if (data == '\'') {
        currentToken->doctypeSystemId = "";
        state = HTMLTokenizer::DocTypeSystemIdentifierSingleQuotedState;
        stateFn = &HTMLTokenizerPrivate::docTypeSystemIdentifierSingleQuotedState;
    } else if (data == '>') {
        Q_EMIT q->parserError(QString("unexpected-char-in-doctype"));
        currentToken->forceQuirks = true;
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else {
        Q_EMIT q->parserError(QString("unexpected-char-in-doctype"));
        currentToken->forceQuirks = true;
        state = HTMLTokenizer::BogusDocTypeState;
        stateFn = &HTMLTokenizerPrivate::bogusDocTypeState;
    }

    return true;
}

bool HTMLTokenizerPrivate::docTypeSystemIdentifierDoubleQuotedState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-doctype"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        currentToken->forceQuirks = true;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (data == '"') {
        state = HTMLTokenizer::AfterDocTypeSystemIdentifierState;
        stateFn = &HTMLTokenizerPrivate::afterDocTypeSystemIdentifierState;
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        currentToken->doctypeSystemId.append(QChar::ReplacementCharacter);
        state = HTMLTokenizer::BeforeDocTypeSystemIdentifierState;
        stateFn = &HTMLTokenizerPrivate::beforeDocTypeSystemIdentifierState;
    } else if (data == '>') {
        Q_EMIT q->parserError(QString("unexpected-end-of-doctype"));
        currentToken->forceQuirks = true;
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else {
        currentToken->doctypeSystemId.append(data);
    }

    return true;
}

bool HTMLTokenizerPrivate::docTypeSystemIdentifierSingleQuotedState()
{
    Q_Q(HTMLTokenizer);

    QChar data;

    if (!consumeStream(data)) {
        Q_EMIT q->parserError(QString("eof-in-doctype"));
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        currentToken->forceQuirks = true;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (data == '\'') {
        state = HTMLTokenizer::AfterDocTypeSystemIdentifierState;
        stateFn = &HTMLTokenizerPrivate::afterDocTypeSystemIdentifierState;
    } else if (data.isNull()) {
        Q_EMIT q->parserError(QString("invalid-codepoint"));
        currentToken->doctypeSystemId.append(QChar::ReplacementCharacter);
        state = HTMLTokenizer::BeforeDocTypeSystemIdentifierState;
        stateFn = &HTMLTokenizerPrivate::beforeDocTypeSystemIdentifierState;
    } else if (data == '>') {
        Q_EMIT q->parserError(QString("unexpected-end-of-doctype"));
        currentToken->forceQuirks = true;
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    }  else {
        currentToken->doctypeSystemId.append(data);
    }

    return true;
}

bool HTMLTokenizerPrivate::afterDocTypeSystemIdentifierState()
{
    Q_Q(HTMLTokenizer);

    QChar data;
    do {
        if (!consumeStream(data)) {
            Q_EMIT q->parserError(QString("eof-in-doctype"));
            state = HTMLTokenizer::DataState;
            stateFn = &HTMLTokenizerPrivate::dataState;
            currentToken->forceQuirks = true;
            emitCurrentTagToken();
            streamUnconsume();
            return true;
        }
    } while (IS_SPACE_CHARACTER(data)); // Ignore all space characters

    if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    } else {
        q->parserError(QString("unexpected-char-in-doctype"));
        currentToken->forceQuirks = true;
        state = HTMLTokenizer::BogusDocTypeState;
        stateFn = &HTMLTokenizerPrivate::bogusDocTypeState;
    }

    return true;
}

bool HTMLTokenizerPrivate::bogusDocTypeState()
{
    Q_Q(HTMLTokenizer);

    QChar data;
    if (!consumeStream(data)) {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
        streamUnconsume();
    } else if (data == '>') {
        state = HTMLTokenizer::DataState;
        stateFn = &HTMLTokenizerPrivate::dataState;
        emitCurrentTagToken();
    }

    return true;
}

bool HTMLTokenizerPrivate::cDataSectionState()
{
    // TODO
    return true;
}

// https://html.spec.whatwg.org/multipage/syntax.html#consume-a-character-reference
QString HTMLTokenizerPrivate::consumeEntity(QChar *allowedChar)
{
    Q_Q(HTMLTokenizer);

    int initalPos = streamPos();
    QString output = QString("&");

    QChar data;
    if (!consumeStream(data) ||
            IS_SPACE_CHARACTER(data) || data == '<' || data == '&' ||
            (allowedChar && data == *allowedChar)) {
        // Not a character reference. No characters are consumed,
        // and nothing is returned. (This is not an error, either.)
        streamUnconsume();
        return QString();
    } else if (data == '#') {
        output.append(data);

        // TODO check this
        consumeStream(data);
        QChar number;
        if (data == 'x' || data == 'X') {
            number = consumeNumberEntity(true);
        } else {
            number = consumeNumberEntity(false);
        }

        if (number.isNull()) {
            q->parserError(QString("expected-numeric-entity"));
            // unconsume all characters
            streamSeek(initalPos);
            return QString();
        }

        return number;
    } else {

    }
    return QString();
}

QMap<int, int> HTMLTokenizerPrivate::initReplacementCharacters()
{
	QMap<int, int> res;
	res.insert( 0x00, 0xFFFD  ); // REPLACEMENT CHARACTER
	res.insert( 0x80, 0x20AC  ); // EURO SIGN (�)
	res.insert( 0x82, 0x201A  ); // SINGLE LOW-9 QUOTATION MARK (�)
	res.insert( 0x83, 0x0192  ); // LATIN SMALL LETTER F WITH HOOK (?)
	res.insert( 0x84, 0x201E  ); // DOUBLE LOW-9 QUOTATION MARK (�)
	res.insert( 0x85, 0x2026  ); // HORIZONTAL ELLIPSIS (�)
	res.insert( 0x86, 0x2020  ); // DAGGER (�)
	res.insert( 0x87, 0x2021  ); // DOUBLE DAGGER (�)
	res.insert( 0x88, 0x02C6  ); // MODIFIER LETTER CIRCUMFLEX ACCENT (?)
	res.insert( 0x89, 0x2030  ); // PER MILLE SIGN (�)
	res.insert( 0x8A, 0x0160  ); // LATIN CAPITAL LETTER S WITH CARON (�)
	res.insert( 0x8B, 0x2039  ); // SINGLE LEFT-POINTING ANGLE QUOTATION MARK (�)
	res.insert( 0x8C, 0x0152  ); // LATIN CAPITAL LIGATURE OE (S)
	res.insert( 0x8E, 0x017D  ); // LATIN CAPITAL LETTER Z WITH CARON (�)
	res.insert( 0x91, 0x2018  ); // LEFT SINGLE QUOTATION MARK (�)
	res.insert( 0x92, 0x2019  ); // RIGHT SINGLE QUOTATION MARK (�)
	res.insert( 0x93, 0x201C  ); // LEFT DOUBLE QUOTATION MARK (�)
	res.insert( 0x94, 0x201D  ); // RIGHT DOUBLE QUOTATION MARK (�)
	res.insert( 0x95, 0x2022  ); // BULLET (�)
	res.insert( 0x96, 0x2013  ); // EN DASH (�)
	res.insert( 0x97, 0x2014  ); // EM DASH (�)
	res.insert( 0x98, 0x02DC  ); // SMALL TILDE (?)
	res.insert( 0x99, 0x2122  ); // TRADE MARK SIGN (�)
	res.insert( 0x9A, 0x0161  ); // LATIN SMALL LETTER S WITH CARON (�)
	res.insert( 0x9B, 0x203A  ); // SINGLE RIGHT-POINTING ANGLE QUOTATION MARK (�)
	res.insert( 0x9C, 0x0153  ); // LATIN SMALL LIGATURE OE (s)
	res.insert( 0x9E, 0x017E  ); // LATIN SMALL LETTER Z WITH CARON (�)
	res.insert( 0x9F, 0x0178  ); // LATIN CAPITAL LETTER Y WITH DIAERESIS (z)
	return res;
}

QChar HTMLTokenizerPrivate::consumeNumberEntity(bool isHex)
{
    Q_Q(HTMLTokenizer);

    QChar ret;
    QString charStack;
    QChar c;
    // TODO check this
    consumeStream(c);
    int lastPos = streamPos();
    if (isHex) {
        while (IS_ASCII_HEX_DIGITS(c) &&
               !streamAtEnd()) {
            charStack.append(c); // store the position to rewind for ;
            lastPos = streamPos();
            // TODO check this
            consumeStream(c);
        }
    } else {
        while (IS_ASCII_DIGITS(c) && // Zero (0) to Nine (9)
               !streamAtEnd()) {
            charStack.append(c);
            lastPos = streamPos(); // store the position to rewind for ;
            // TODO check this
            consumeStream(c);
        }
    }

    // No char was found return null to unconsume
    if (charStack.isNull()) {
        return QChar::Null;
    }

    // Discard the ; if present. Otherwise, put it back on the queue and
    // invoke parseError on parser.
    if (c != ';') {
        q->parserError(QString("numeric-entity-without-semicolon"));
        streamSeek(lastPos);
    }

    // Convert the number using the proper base
    bool ok;
    int charAsInt = charStack.toInt(&ok, isHex ? 16 : 10);
    if (!ok) {
        // TODO error
    }

	static QMap<int, int> replacementCharacters = initReplacementCharacters();

    // Certain characters get replaced with others
    QMap<int,int>::ConstIterator it = replacementCharacters.constFind(charAsInt);
    if (it != replacementCharacters.constEnd()) {
        ret = it.value();
        q->parserError(QString("illegal-codepoint-for-numeric-entity: %1").arg(charStack));
    } else if ((charAsInt >= 0xD800 && charAsInt <= 0xDFFF) || charAsInt > 0x10FFFF) {
        ret = QChar::ReplacementCharacter;
        q->parserError(QString("illegal-codepoint-for-numeric-entity: %1").arg(charStack));
    } else {
        if ((0x0001 <= charAsInt && charAsInt <= 0x0008) ||
                (0x000E <= charAsInt && charAsInt <= 0x001F) ||
                (0x007F <= charAsInt && charAsInt <= 0x009F) ||
                (0xFDD0 <= charAsInt && charAsInt <= 0xFDEF) ||
                (charAsInt == 0x000B || charAsInt == 0xFFFE || charAsInt == 0xFFFF || charAsInt == 0x1FFFE ||
                 charAsInt == 0x1FFFF || charAsInt == 0x2FFFE || charAsInt == 0x2FFFF || charAsInt == 0x3FFFE ||
                 charAsInt == 0x3FFFF || charAsInt == 0x4FFFE || charAsInt == 0x4FFFF || charAsInt == 0x5FFFE ||
                 charAsInt == 0x5FFFF || charAsInt == 0x6FFFE || charAsInt == 0x6FFFF || charAsInt == 0x7FFFE ||
                 charAsInt == 0x7FFFF || charAsInt == 0x8FFFE || charAsInt == 0x8FFFF || charAsInt == 0x9FFFE ||
                 charAsInt == 0x9FFFF || charAsInt == 0xAFFFE || charAsInt == 0xAFFFF || charAsInt == 0xBFFFE ||
                 charAsInt == 0xBFFFF || charAsInt == 0xCFFFE || charAsInt == 0xCFFFF || charAsInt == 0xDFFFE ||
                 charAsInt == 0xDFFFF || charAsInt == 0xEFFFE || charAsInt == 0xEFFFF || charAsInt == 0xFFFFE ||
                 charAsInt == 0xFFFFF || charAsInt == 0x10FFFE || charAsInt == 0x10FFFF)) {
            q->parserError(QString("illegal-codepoint-for-numeric-entity: %1").arg(charStack));
            ret = charAsInt;
        }
    }

    return ret;
}

void HTMLTokenizerPrivate::emitCurrentTagToken( )
{
    Q_Q(HTMLTokenizer);

//    qDebug() << "emitCurrentTagToken" << currentToken;
    //HTMLToken *token = currentToken;
    if ( currentToken->type == HTMLToken::EndTagToken) {
        if (!currentToken->data.isEmpty()) {
            Q_EMIT q->parserError(QString("attributes-in-end-tag"));
        }

        if ( currentToken->selfClosing) {
            Q_EMIT q->parserError(QString("self-closing-flag-on-end-tag"));
        }
    }
	if ( currentToken->type == HTMLToken::StartTagToken ) {
		currentToken->tagAsPure = html.mid( tagStart, ( htmlPos + 1 ) - tagStart );
	}

    Q_EMIT q->token( currentToken );

    currentToken.clear();
}
