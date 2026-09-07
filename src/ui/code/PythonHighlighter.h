#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

namespace mn::ui {

/// Syntax highlighting for Python, with Manim's own vocabulary picked out.
///
/// Manim names are not merely coloured as identifiers: a mobject, an animation,
/// a scene, a colour and a rate function each get their own treatment, so the
/// shape of a scene is legible at a glance without reading it.
class PythonHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit PythonHighlighter(QTextDocument *document = nullptr);

    /// True while the block ends inside a triple-quoted string.
    enum BlockState {
        NotInString = 0,
        InSingleQuotedBlock = 1,   ///< inside '''...'''
        InDoubleQuotedBlock = 2,   ///< inside """..."""
    };

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    void buildFormats();
    void buildRules();

    /// Colour the run of triple-quoted string that this block sits in, if any.
    /// Returns the offset past it, or -1 if the whole block was consumed.
    bool applyBlockStrings(const QString &text);

    /// Colour identifiers by which part of Manim's vocabulary they belong to.
    void applyVocabulary(const QString &text);

    QVector<Rule> m_rules;

    QTextCharFormat m_keyword;
    QTextCharFormat m_builtin;
    QTextCharFormat m_definition;
    QTextCharFormat m_string;
    QTextCharFormat m_number;
    QTextCharFormat m_comment;
    QTextCharFormat m_decorator;
    QTextCharFormat m_operator;
    QTextCharFormat m_self;

    QTextCharFormat m_mobject;
    QTextCharFormat m_animation;
    QTextCharFormat m_scene;
    QTextCharFormat m_color;
    QTextCharFormat m_constant;
    QTextCharFormat m_rateFunction;

    QRegularExpression m_identifier;
    QRegularExpression m_tripleSingle;
    QRegularExpression m_tripleDouble;
};

} // namespace mn::ui
