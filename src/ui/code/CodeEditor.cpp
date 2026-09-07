#include "CodeEditor.h"

#include "ManimVocabulary.h"
#include "PythonHighlighter.h"
#include "Theme.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStringListModel>
#include <QTextBlock>

namespace mn::ui {
namespace {

constexpr int kGutterPadding = 12;
constexpr int kMinimumCompletionPrefix = 2;

/// Brackets that are matched against each other.
constexpr QChar kOpeners[] = {u'(', u'[', u'{'};
constexpr QChar kClosers[] = {u')', u']', u'}'};

int openerIndex(QChar c)
{
    for (int i = 0; i < 3; ++i) {
        if (kOpeners[i] == c)
            return i;
    }
    return -1;
}

int closerIndex(QChar c)
{
    for (int i = 0; i < 3; ++i) {
        if (kClosers[i] == c)
            return i;
    }
    return -1;
}

/// The gutter is a plain widget; the editor paints it, because only the editor
/// knows where the blocks are.
class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor)
        : QWidget(editor)
        , m_editor(editor)
    {
    }

    QSize sizeHint() const override { return {m_editor->lineNumberAreaWidth(), 0}; }

protected:
    void paintEvent(QPaintEvent *event) override { m_editor->paintLineNumbers(event); }

private:
    CodeEditor *m_editor;
};

QFont monospaceFont()
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(13);
    font.setFixedPitch(true);
    return font;
}

} // namespace

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    const theme::Palette &p = theme::palette();

    setFont(monospaceFont());
    setTabStopDistance(QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' ')) * kIndentWidth);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setFrameShape(QFrame::NoFrame);
    setCursorWidth(2);
    document()->setDocumentMargin(8);

    setStyleSheet(QStringLiteral("QPlainTextEdit {"
                                 "  background: %1;"
                                 "  color: %2;"
                                 "  border: none;"
                                 "  selection-background-color: %3;"
                                 "  selection-color: %2;"
                                 "}")
                      .arg(p.surface.name(), p.text.name(),
                           theme::mix(p.window, p.accent, 0.35).name()));

    m_lineNumberArea = new LineNumberArea(this);
    m_highlighter = new PythonHighlighter(document());

    m_vocabulary = python::keywords() + python::builtins() + python::softKeywords() + manim::all();
    m_vocabulary.removeDuplicates();
    m_vocabulary.sort(Qt::CaseInsensitive);

    m_completionModel = new QStringListModel(m_vocabulary, this);
    m_completer = new QCompleter(m_completionModel, this);
    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseSensitive);
    m_completer->setWrapAround(false);
    m_completer->popup()->setStyleSheet(
        QStringLiteral("QAbstractItemView {"
                       "  background: %1;"
                       "  color: %2;"
                       "  border: 1px solid %3;"
                       "  border-radius: 6px;"
                       "  padding: 4px;"
                       "  outline: none;"
                       "  selection-background-color: %4;"
                       "}")
            .arg(p.surfaceRaised.name(), p.text.name(), p.border.name(),
                 theme::mix(p.surfaceHover, p.accent, 0.35).name()));

    connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
            this, &CodeEditor::insertCompletion);
    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this] {
        highlightCurrentLineAndBrackets();
        const QTextCursor cursor = textCursor();
        Q_EMIT cursorPositionDescribed(cursor.blockNumber() + 1, cursor.positionInBlock() + 1);
    });

    updateLineNumberAreaWidth();
    highlightCurrentLineAndBrackets();
}

int CodeEditor::currentLine() const
{
    return textCursor().blockNumber() + 1;
}

int CodeEditor::lineCount() const
{
    return blockCount();
}

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    for (int max = qMax(1, blockCount()); max >= 10; max /= 10)
        ++digits;
    return kGutterPadding * 2 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void CodeEditor::updateLineNumberAreaWidth()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy != 0)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth();
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect area = contentsRect();
    m_lineNumberArea->setGeometry(area.left(), area.top(), lineNumberAreaWidth(), area.height());
}

void CodeEditor::paintLineNumbers(QPaintEvent *event)
{
    const theme::Palette &p = theme::palette();

    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), p.window);

    QTextBlock block = firstVisibleBlock();
    int number = block.blockNumber();
    int top = int(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + int(blockBoundingRect(block).height());
    const int active = textCursor().blockNumber();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.setPen(number == active ? p.text : p.textFaint);
            painter.drawText(0, top, m_lineNumberArea->width() - kGutterPadding,
                             fontMetrics().height(), Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(number + 1));
        }
        block = block.next();
        top = bottom;
        bottom = top + int(blockBoundingRect(block).height());
        ++number;
    }
}

int CodeEditor::matchingBracket(int position) const
{
    const QString text = document()->toPlainText();
    if (position < 0 || position >= text.length())
        return -1;

    const QChar c = text.at(position);
    const int opener = openerIndex(c);
    const int closer = closerIndex(c);

    if (opener >= 0) {
        int depth = 0;
        for (int i = position; i < text.length(); ++i) {
            if (text.at(i) == kOpeners[opener])
                ++depth;
            else if (text.at(i) == kClosers[opener] && --depth == 0)
                return i;
        }
    } else if (closer >= 0) {
        int depth = 0;
        for (int i = position; i >= 0; --i) {
            if (text.at(i) == kClosers[closer])
                ++depth;
            else if (text.at(i) == kOpeners[closer] && --depth == 0)
                return i;
        }
    }
    return -1;
}

void CodeEditor::highlightCurrentLineAndBrackets()
{
    const theme::Palette &p = theme::palette();
    QList<QTextEdit::ExtraSelection> selections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection line;
        line.format.setBackground(theme::mix(p.surface, p.surfaceHover, 0.7));
        line.format.setProperty(QTextFormat::FullWidthSelection, true);
        line.cursor = textCursor();
        line.cursor.clearSelection();
        selections.append(line);
    }

    // Look at the character on each side of the cursor, as editors do, so a
    // bracket lights up whether the cursor sits before or after it.
    const int position = textCursor().position();
    for (const int candidate : {position, position - 1}) {
        const int partner = matchingBracket(candidate);
        if (partner < 0)
            continue;

        QTextCharFormat format;
        format.setForeground(p.accent);
        format.setBackground(theme::mix(p.surface, p.accent, 0.22));
        format.setFontWeight(QFont::Bold);

        for (const int at : {candidate, partner}) {
            QTextEdit::ExtraSelection bracket;
            bracket.format = format;
            bracket.cursor = textCursor();
            bracket.cursor.setPosition(at);
            bracket.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            selections.append(bracket);
        }
        break;
    }

    setExtraSelections(selections);
}

QString CodeEditor::wordUnderCursor() const
{
    QTextCursor cursor = textCursor();
    const QString line = cursor.block().text();
    int end = cursor.positionInBlock();
    int start = end;
    while (start > 0) {
        const QChar c = line.at(start - 1);
        if (!c.isLetterOrNumber() && c != QLatin1Char('_'))
            break;
        --start;
    }
    return line.mid(start, end - start);
}

QString CodeEditor::currentIndent() const
{
    const QString line = textCursor().block().text();
    int spaces = 0;
    while (spaces < line.length() && line.at(spaces) == QLatin1Char(' '))
        ++spaces;
    return QString(spaces, QLatin1Char(' '));
}

void CodeEditor::insertCompletion(const QString &completion)
{
    QTextCursor cursor = textCursor();
    const int extra = int(completion.length() - m_completer->completionPrefix().length());
    cursor.movePosition(QTextCursor::Left);
    cursor.movePosition(QTextCursor::EndOfWord);
    cursor.insertText(completion.right(extra));
    setTextCursor(cursor);
}

void CodeEditor::refreshDocumentWords()
{
    // Offer identifiers the file itself defines, alongside the fixed
    // vocabulary, so a scene's own names complete too.
    static const QRegularExpression identifier(QStringLiteral(R"(\b[A-Za-z_]\w{2,}\b)"));

    QStringList words = m_vocabulary;
    auto matches = identifier.globalMatch(document()->toPlainText());
    while (matches.hasNext())
        words.append(matches.next().captured());

    words.removeDuplicates();
    words.sort(Qt::CaseInsensitive);
    m_completionModel->setStringList(words);
}

bool CodeEditor::handleReturn()
{
    QTextCursor cursor = textCursor();
    const QString line = cursor.block().text();
    const QString before = line.left(cursor.positionInBlock()).trimmed();

    QString indent = currentIndent();
    // A line ending in a colon opens a block, so the next line steps in.
    if (before.endsWith(QLatin1Char(':')))
        indent += QString(kIndentWidth, QLatin1Char(' '));

    cursor.beginEditBlock();
    cursor.insertText(QStringLiteral("\n") + indent);
    cursor.endEditBlock();
    setTextCursor(cursor);
    return true;
}

bool CodeEditor::handleIndent(bool forward)
{
    QTextCursor cursor = textCursor();

    if (!cursor.hasSelection()) {
        if (forward) {
            // Tab moves to the next multiple of the indent width, not a fixed
            // four spaces, so columns stay aligned.
            const int column = cursor.positionInBlock();
            cursor.insertText(QString(kIndentWidth - (column % kIndentWidth), QLatin1Char(' ')));
            return true;
        }
        // Shift+Tab with no selection unindents the line.
    }

    const int start = cursor.selectionStart();
    const int end = cursor.selectionEnd();

    QTextCursor edit(document());
    edit.setPosition(start);
    const int firstBlock = edit.blockNumber();
    edit.setPosition(end);
    const int lastBlock = edit.blockNumber();

    edit.beginEditBlock();
    for (int number = firstBlock; number <= lastBlock; ++number) {
        QTextBlock block = document()->findBlockByNumber(number);
        edit.setPosition(block.position());

        if (forward) {
            edit.insertText(QString(kIndentWidth, QLatin1Char(' ')));
        } else {
            const QString text = block.text();
            int remove = 0;
            while (remove < kIndentWidth && remove < text.length()
                   && text.at(remove) == QLatin1Char(' ')) {
                ++remove;
            }
            if (remove > 0) {
                edit.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, remove);
                edit.removeSelectedText();
            }
        }
    }
    edit.endEditBlock();
    return true;
}

bool CodeEditor::handleBackspace()
{
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
        return false;

    const int column = cursor.positionInBlock();
    if (column == 0 || column % kIndentWidth != 0)
        return false;

    // Only collapse a whole indent level when everything before the cursor is
    // whitespace; mid-line, backspace should delete one character.
    const QString before = cursor.block().text().left(column);
    if (!before.trimmed().isEmpty())
        return false;

    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, kIndentWidth);
    cursor.removeSelectedText();
    return true;
}

bool CodeEditor::handleCommentToggle()
{
    QTextCursor cursor = textCursor();
    const int start = cursor.selectionStart();
    const int end = cursor.selectionEnd();

    QTextCursor edit(document());
    edit.setPosition(start);
    const int firstBlock = edit.blockNumber();
    edit.setPosition(end);
    const int lastBlock = edit.blockNumber();

    // If every non-empty line is already commented, uncomment them all.
    bool allCommented = true;
    for (int number = firstBlock; number <= lastBlock; ++number) {
        const QString text = document()->findBlockByNumber(number).text().trimmed();
        if (!text.isEmpty() && !text.startsWith(QLatin1Char('#'))) {
            allCommented = false;
            break;
        }
    }

    edit.beginEditBlock();
    for (int number = firstBlock; number <= lastBlock; ++number) {
        QTextBlock block = document()->findBlockByNumber(number);
        const QString text = block.text();
        if (text.trimmed().isEmpty())
            continue;

        if (allCommented) {
            const qsizetype hash = text.indexOf(QLatin1Char('#'));
            edit.setPosition(block.position() + int(hash));
            int remove = 1;
            if (hash + 1 < text.length() && text.at(hash + 1) == QLatin1Char(' '))
                remove = 2;
            edit.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, remove);
            edit.removeSelectedText();
        } else {
            int indent = 0;
            while (indent < text.length() && text.at(indent) == QLatin1Char(' '))
                ++indent;
            edit.setPosition(block.position() + indent);
            edit.insertText(QStringLiteral("# "));
        }
    }
    edit.endEditBlock();
    return true;
}

void CodeEditor::focusInEvent(QFocusEvent *event)
{
    m_completer->setWidget(this);
    QPlainTextEdit::focusInEvent(event);
}

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    QAbstractItemView *popup = m_completer->popup();

    if (popup->isVisible()) {
        // The popup owns these keys while it is up.
        switch (event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            event->ignore();
            return;
        default:
            break;
        }
    }

    const bool completionShortcut =
        event->modifiers().testFlag(Qt::ControlModifier) && event->key() == Qt::Key_Space;

    if (!completionShortcut) {
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (!event->modifiers().testFlag(Qt::ShiftModifier) && handleReturn())
                return;
            break;
        case Qt::Key_Tab:
            if (handleIndent(true))
                return;
            break;
        case Qt::Key_Backtab:
            if (handleIndent(false))
                return;
            break;
        case Qt::Key_Backspace:
            if (handleBackspace())
                return;
            break;
        case Qt::Key_Slash:
            if (event->modifiers().testFlag(Qt::ControlModifier) && handleCommentToggle())
                return;
            break;
        default:
            break;
        }

        QPlainTextEdit::keyPressEvent(event);
    }

    const QString prefix = wordUnderCursor();
    const bool longEnough = prefix.length() >= kMinimumCompletionPrefix;

    if (!completionShortcut && !longEnough) {
        popup->hide();
        return;
    }

    refreshDocumentWords();
    if (prefix != m_completer->completionPrefix()) {
        m_completer->setCompletionPrefix(prefix);
        popup->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    }

    if (m_completer->completionCount() == 0
        || (m_completer->completionCount() == 1 && m_completer->currentCompletion() == prefix)) {
        popup->hide();
        return;
    }

    QRect box = cursorRect();
    box.setWidth(popup->sizeHintForColumn(0) + popup->verticalScrollBar()->sizeHint().width() + 24);
    m_completer->complete(box);
}

} // namespace mn::ui
