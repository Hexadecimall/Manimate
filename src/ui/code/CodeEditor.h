#pragma once

#include <QPlainTextEdit>

class QCompleter;
class QStringListModel;

namespace mn::ui {

class PythonHighlighter;

/// A Python editor that knows what Manim is.
///
/// Everything here is the ordinary furniture of a code editor — line numbers,
/// the current line, matching brackets, indentation that behaves — plus a
/// completer whose vocabulary is Python's and Manim's together, so the names
/// that make up a scene can be typed without remembering their exact spelling.
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

    /// Spaces per indent level. Python's own convention.
    static constexpr int kIndentWidth = 4;

    /// Width of the line number column, for the gutter to paint into.
    int lineNumberAreaWidth() const;
    void paintLineNumbers(QPaintEvent *event);

    /// Line the cursor is on, and the total, both 1-based.
    int currentLine() const;
    int lineCount() const;

Q_SIGNALS:
    void cursorPositionDescribed(int line, int column);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLineAndBrackets();
    void insertCompletion(const QString &completion);

    /// The word being typed at the cursor, used as the completion prefix.
    QString wordUnderCursor() const;

    /// Indentation of the line the cursor is on, as a string of spaces.
    QString currentIndent() const;

    /// Handle Return, Tab, Backtab and Backspace the way a code editor should.
    /// Returns true when the key was consumed.
    bool handleReturn();
    bool handleIndent(bool forward);
    bool handleBackspace();
    bool handleCommentToggle();

    /// Position of the bracket matching the one at `position`, or -1.
    int matchingBracket(int position) const;

    /// Refresh the completer's word list from the document's own identifiers.
    void refreshDocumentWords();

    QWidget *m_lineNumberArea = nullptr;
    PythonHighlighter *m_highlighter = nullptr;
    QCompleter *m_completer = nullptr;
    QStringListModel *m_completionModel = nullptr;
    QStringList m_vocabulary;
};

} // namespace mn::ui
