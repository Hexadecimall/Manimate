#include "PythonHighlighter.h"

#include "ManimVocabulary.h"
#include "Theme.h"

namespace mn::ui {

PythonHighlighter::PythonHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
    , m_identifier(QStringLiteral(R"(\b[A-Za-z_]\w*\b)"))
    , m_tripleSingle(QStringLiteral("'''"))
    , m_tripleDouble(QStringLiteral("\"\"\""))
{
    buildFormats();
    buildRules();
}

void PythonHighlighter::buildFormats()
{
    const theme::Palette &p = theme::palette();

    m_keyword.setForeground(theme::mix(p.violet, p.text, 0.25));
    m_keyword.setFontWeight(QFont::DemiBold);

    m_builtin.setForeground(p.teal);

    m_definition.setForeground(p.accent);
    m_definition.setFontWeight(QFont::DemiBold);

    m_string.setForeground(p.success);

    m_number.setForeground(theme::mix(p.warning, p.text, 0.1));

    m_comment.setForeground(p.textFaint);
    m_comment.setFontItalic(true);

    m_decorator.setForeground(p.warning);
    m_decorator.setFontItalic(true);

    m_operator.setForeground(p.textMuted);

    m_self.setForeground(p.danger);
    m_self.setFontItalic(true);

    // Manim's own vocabulary.
    m_mobject.setForeground(p.accent);
    m_animation.setForeground(theme::mix(p.violet, p.text, 0.45));
    m_animation.setFontWeight(QFont::DemiBold);
    m_scene.setForeground(p.teal);
    m_scene.setFontWeight(QFont::DemiBold);
    m_color.setForeground(p.warning);
    m_constant.setForeground(theme::mix(p.warning, p.danger, 0.35));
    m_rateFunction.setForeground(p.teal);
    m_rateFunction.setFontItalic(true);
}

void PythonHighlighter::buildRules()
{
    auto add = [this](const QString &pattern, const QTextCharFormat &format) {
        m_rules.append({QRegularExpression(pattern), format});
    };

    // Numbers: decimal, float, exponent, hex, binary, octal, complex, with
    // Python's underscore separators.
    add(QStringLiteral(R"(\b0[xX][0-9a-fA-F_]+\b)"), m_number);
    add(QStringLiteral(R"(\b0[bB][01_]+\b)"), m_number);
    add(QStringLiteral(R"(\b0[oO][0-7_]+\b)"), m_number);
    add(QStringLiteral(R"(\b\d[\d_]*(\.[\d_]*)?([eE][+-]?\d+)?[jJ]?\b)"), m_number);
    add(QStringLiteral(R"(\.\d[\d_]*([eE][+-]?\d+)?[jJ]?\b)"), m_number);

    add(QStringLiteral(R"([+\-*/%=<>!&|^~@]+)"), m_operator);

    // The name being defined, rather than the keyword introducing it.
    add(QStringLiteral(R"(\bdef\s+([A-Za-z_]\w*))"), m_definition);
    add(QStringLiteral(R"(\bclass\s+([A-Za-z_]\w*))"), m_definition);

    add(QStringLiteral(R"(^\s*@[A-Za-z_][\w.]*)"), m_decorator);
}

bool PythonHighlighter::applyBlockStrings(const QString &text)
{
    // Continue a triple-quoted string opened on an earlier line, then look for
    // one opening on this line. Returns true if the block ends inside a string.
    int state = previousBlockState();
    if (state != InSingleQuotedBlock && state != InDoubleQuotedBlock)
        state = NotInString;

    int index = 0;       // where the next opener is looked for
    int runStart = 0;    // where the string currently being coloured began
    int searchFrom = 0;  // where the closing delimiter is looked for

    while (true) {
        if (state == NotInString) {
            const QRegularExpressionMatch single = m_tripleSingle.match(text, index);
            const QRegularExpressionMatch doubl = m_tripleDouble.match(text, index);

            int at = -1;
            int opened = NotInString;
            if (single.hasMatch() && (!doubl.hasMatch() || single.capturedStart() < doubl.capturedStart())) {
                at = int(single.capturedStart());
                opened = InSingleQuotedBlock;
            } else if (doubl.hasMatch()) {
                at = int(doubl.capturedStart());
                opened = InDoubleQuotedBlock;
            }

            if (at < 0)
                break;

            // A triple quote after a hash on the same line is inside a comment
            // and opens nothing.
            if (text.mid(index, at - index).contains(QLatin1Char('#')))
                break;

            state = opened;
            runStart = at;
            // The delimiter that opened the string cannot also close it.
            searchFrom = at + 3;
        } else {
            const QRegularExpression &closer =
                state == InSingleQuotedBlock ? m_tripleSingle : m_tripleDouble;
            const QRegularExpressionMatch end = closer.match(text, searchFrom);
            if (!end.hasMatch()) {
                setFormat(runStart, text.length() - runStart, m_string);
                setCurrentBlockState(state);
                return true;
            }

            const int stop = int(end.capturedEnd());
            setFormat(runStart, stop - runStart, m_string);
            state = NotInString;
            index = stop;
        }
    }

    setCurrentBlockState(NotInString);
    return false;
}

void PythonHighlighter::applyVocabulary(const QString &text)
{
    auto matches = m_identifier.globalMatch(text);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const QString word = match.captured();
        const int at = int(match.capturedStart());
        const int length = int(match.capturedLength());

        // An attribute is named by its owner, not by the vocabulary: `x.smooth`
        // is not the rate function.
        const bool afterDot = at > 0 && text.at(at - 1) == QLatin1Char('.');

        if (python::isKeyword(word)) {
            setFormat(at, length, m_keyword);
        } else if (word == QLatin1String("self") || word == QLatin1String("cls")) {
            setFormat(at, length, m_self);
        } else if (manim::isScene(word)) {
            setFormat(at, length, m_scene);
        } else if (manim::isAnimation(word)) {
            setFormat(at, length, m_animation);
        } else if (manim::isMobject(word)) {
            setFormat(at, length, m_mobject);
        } else if (manim::isColor(word)) {
            setFormat(at, length, m_color);
        } else if (manim::isConstant(word)) {
            setFormat(at, length, m_constant);
        } else if (!afterDot && manim::isRateFunction(word)) {
            setFormat(at, length, m_rateFunction);
        } else if (!afterDot && python::isBuiltin(word)) {
            setFormat(at, length, m_builtin);
        }
    }
}

void PythonHighlighter::highlightBlock(const QString &text)
{
    // Order matters: identifiers and numbers first, then quoted text over the
    // top of them, then comments over everything.
    applyVocabulary(text);

    for (const Rule &rule : std::as_const(m_rules)) {
        auto matches = rule.pattern.globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            // Rules with a capture group colour the group, not the whole match.
            const int group = match.lastCapturedIndex() >= 1 ? 1 : 0;
            setFormat(int(match.capturedStart(group)), int(match.capturedLength(group)), rule.format);
        }
    }

    if (applyBlockStrings(text))
        return;

    // Single-line strings, including prefixed forms such as f"" and rb''.
    static const QRegularExpression quoted(
        QStringLiteral(R"((?:[rRbBuUfF]{0,3})(?:"(?:[^"\\\n]|\\.)*"|'(?:[^'\\\n]|\\.)*'))"));
    auto strings = quoted.globalMatch(text);
    while (strings.hasNext()) {
        const QRegularExpressionMatch match = strings.next();
        setFormat(int(match.capturedStart()), int(match.capturedLength()), m_string);
    }

    // A comment runs to the end of the line, unless the hash is inside a string.
    for (int i = 0; i < text.length(); ++i) {
        if (text.at(i) != QLatin1Char('#'))
            continue;
        if (format(i) == m_string)
            continue;
        setFormat(i, text.length() - i, m_comment);
        break;
    }
}

} // namespace mn::ui
