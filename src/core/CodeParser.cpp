#include "CodeParser.h"

#include "Catalog.h"
#include "Document.h"

#include <QColor>
#include <QHash>
#include <QPointF>
#include <QRegularExpression>

namespace mn::codeparser {
namespace {

/// Split a call's arguments on top-level commas, so nested calls and lists
/// survive intact.
QStringList splitArguments(const QString &text)
{
    QStringList parts;
    int depth = 0;
    QChar quote;
    QString current;

    for (const QChar c : text) {
        if (!quote.isNull()) {
            current.append(c);
            if (c == quote)
                quote = QChar();
            continue;
        }
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            quote = c;
            current.append(c);
            continue;
        }
        if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{'))
            ++depth;
        else if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}'))
            --depth;

        if (c == QLatin1Char(',') && depth == 0) {
            parts.append(current.trimmed());
            current.clear();
            continue;
        }
        current.append(c);
    }
    if (!current.trimmed().isEmpty())
        parts.append(current.trimmed());
    return parts;
}

QString unquote(QString text)
{
    text = text.trimmed();
    if (text.size() >= 2
        && ((text.startsWith(QLatin1Char('"')) && text.endsWith(QLatin1Char('"')))
            || (text.startsWith(QLatin1Char('\'')) && text.endsWith(QLatin1Char('\''))))) {
        text = text.mid(1, text.size() - 2);
    }
    text.replace(QLatin1String("\\n"), QLatin1String("\n"));
    text.replace(QLatin1String("\\\""), QLatin1String("\""));
    text.replace(QLatin1String("\\\\"), QLatin1String("\\"));
    return text;
}

/// `[x, y, 0]` as a point. Returns false for anything else.
bool parsePoint(const QString &text, QPointF *out)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^\s*\[\s*(-?[\d.eE+-]+)\s*,\s*(-?[\d.eE+-]+)\s*(?:,\s*-?[\d.eE+-]+\s*)?\]\s*$)"));
    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch())
        return false;
    *out = QPointF(match.captured(1).toDouble(), match.captured(2).toDouble());
    return true;
}

QVariant valueFor(const catalog::ParamSpec &spec, const QString &literal)
{
    switch (spec.type) {
    case catalog::ParamType::Number:
        return literal.trimmed().toDouble();
    case catalog::ParamType::Integer:
        return literal.trimmed().toInt();
    case catalog::ParamType::Boolean:
        return literal.trimmed() == QLatin1String("True");
    case catalog::ParamType::Color:
        return QColor::fromString(unquote(literal));
    case catalog::ParamType::Point: {
        QPointF point;
        if (parsePoint(literal, &point))
            return point;
        return {};
    }
    case catalog::ParamType::Text:
    case catalog::ParamType::Choice:
        return unquote(literal);
    }
    return {};
}

const catalog::MobjectSpec *mobjectByPythonName(const QString &name)
{
    for (const catalog::MobjectSpec &spec : catalog::mobjects()) {
        if (spec.pythonName == name)
            return &spec;
    }
    return nullptr;
}

const catalog::AnimationSpec *animationByPythonName(const QString &name)
{
    for (const catalog::AnimationSpec &spec : catalog::animations()) {
        if (spec.pythonName == name)
            return &spec;
    }
    return nullptr;
}

/// A statement and the line it started on. A call split across several lines
/// is one statement, which is how the generator writes anything with more than
/// one animation in it.
struct LogicalLine
{
    QString text;
    int number = 0;
};

QVector<LogicalLine> logicalLines(const QString &source)
{
    QVector<LogicalLine> result;
    const QStringList raw = source.split(QLatin1Char('\n'));

    QString pending;
    int startedAt = 0;
    int depth = 0;

    for (int i = 0; i < raw.size(); ++i) {
        const QString line = raw.at(i);
        const QString trimmed = line.trimmed();

        if (depth == 0) {
            if (trimmed.isEmpty())
                continue;
            startedAt = i + 1;
            pending = trimmed;
        } else {
            // Continuations are joined with a space, so splitting on commas
            // later sees one argument list rather than several fragments.
            pending += QLatin1Char(' ') + trimmed;
        }

        QChar quote;
        for (const QChar c : trimmed) {
            if (!quote.isNull()) {
                if (c == quote)
                    quote = QChar();
                continue;
            }
            if (c == QLatin1Char('"') || c == QLatin1Char('\''))
                quote = c;
            else if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{'))
                ++depth;
            else if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}'))
                --depth;
        }

        if (depth <= 0) {
            depth = 0;
            // A trailing comma before the closing bracket is legal Python and
            // would otherwise leave an empty argument behind.
            result.append({pending.trimmed(), startedAt});
            pending.clear();
        }
    }

    if (!pending.trimmed().isEmpty())
        result.append({pending.trimmed(), startedAt});

    return result;
}

} // namespace

Result parse(const QString &source, Document *document)
{
    Result result;
    if (!document) {
        result.error = QStringLiteral("No document to read into.");
        return result;
    }

    static const QRegularExpression classPattern(
        QStringLiteral(R"(^\s*class\s+(\w+)\s*\(\s*\w*Scene\w*\s*\)\s*:)"));
    static const QRegularExpression assignPattern(
        QStringLiteral(R"(^\s*(\w+)\s*=\s*(\w+)\s*\((.*)\)\s*$)"));
    static const QRegularExpression methodPattern(
        QStringLiteral(R"(^\s*(\w+)\.(\w+)\s*\((.*)\)\s*$)"));
    static const QRegularExpression playPattern(QStringLiteral(R"(^\s*self\.play\s*\((.*)\)\s*$)"));
    static const QRegularExpression waitPattern(
        QStringLiteral(R"(^\s*self\.wait\s*\(\s*([\d.]*)\s*\)\s*$)"));
    static const QRegularExpression addPattern(QStringLiteral(R"(^\s*self\.add\s*\((.*)\)\s*$)"));
    static const QRegularExpression soundPattern(
        QStringLiteral(R"(^\s*self\.add_sound\s*\((.*)\)\s*$)"));
    static const QRegularExpression animationPattern(
        QStringLiteral(R"(^\s*(\w+)\s*\((.*)\)\s*$)"));
    static const QRegularExpression animatePattern(
        QStringLiteral(R"(^\s*(\w+)\.animate\s*\(([^)]*)\)\s*\.(\w+)\s*\((.*)\)\s*$)"));
    static const QRegularExpression successionPattern(
        QStringLiteral(R"(^\s*Succession\s*\(\s*Wait\s*\(\s*([\d.]+)\s*\)\s*,\s*(.*)\)\s*$)"));

    Document parsed;
    parsed.metadata = document->metadata;
    parsed.render = document->render;
    parsed.sceneClassName = document->sceneClassName;
    parsed.assets = document->assets;

    Track track;
    track.name = QStringLiteral("Track 1");
    parsed.timeline.tracks.append(track);

    QHash<QString, ObjectId> variables;
    double cursor = 0.0;
    bool sawClass = false;

    const QVector<LogicalLine> lines = logicalLines(source);

    // One animation expression becomes one clip, laid at `cursor`.
    auto readAnimation = [&](QString expression, double clusterStart, int lane) -> double {
        double offset = 0.0;
        const QRegularExpressionMatch succession = successionPattern.match(expression);
        if (succession.hasMatch()) {
            offset = succession.captured(1).toDouble();
            expression = succession.captured(2).trimmed();
            if (expression.endsWith(QLatin1Char(')')))
                expression.chop(0);
        }

        QString objectName;
        QString animationName;
        QStringList arguments;

        const QRegularExpressionMatch animate = animatePattern.match(expression);
        if (animate.hasMatch()) {
            objectName = animate.captured(1);
            const QString method = animate.captured(3);
            animationName = method == QLatin1String("shift")      ? QStringLiteral("Shift")
                            : method == QLatin1String("scale")    ? QStringLiteral("ScaleBy")
                            : method == QLatin1String("set_color") ? QStringLiteral("Recolor")
                            : method == QLatin1String("move_to")  ? QStringLiteral("MoveTo")
                            : method == QLatin1String("set_opacity") ? QStringLiteral("SetOpacity")
                                                                    : QString();
            arguments = splitArguments(animate.captured(2));
            arguments.append(animate.captured(4));
        } else {
            const QRegularExpressionMatch call = animationPattern.match(expression);
            if (!call.hasMatch())
                return 0.0;
            animationName = call.captured(1);
            arguments = splitArguments(call.captured(2));
            if (!arguments.isEmpty() && !arguments.first().contains(QLatin1Char('=')))
                objectName = arguments.takeFirst();
        }

        const catalog::AnimationSpec *spec = animationByPythonName(animationName);
        if (!spec)
            return 0.0;

        Clip clip;
        clip.objectId = variables.value(objectName, kInvalidObjectId);
        clip.type = spec->id;
        clip.start = clusterStart + offset;
        clip.duration = spec->defaultDuration;
        clip.track = lane;
        clip.params = catalog::defaultParams(*spec);

        for (const QString &argument : arguments) {
            const int equals = int(argument.indexOf(QLatin1Char('=')));
            if (equals < 0) {
                // A positional value belongs to the animation's first parameter.
                if (!spec->params.isEmpty()) {
                    const QVariant value = valueFor(spec->params.first(), argument);
                    if (value.isValid())
                        clip.params.insert(spec->params.first().id, value);
                }
                continue;
            }

            const QString key = argument.left(equals).trimmed();
            QString literal = argument.mid(equals + 1).trimmed();

            if (key == QLatin1String("run_time")) {
                clip.duration = literal.toDouble();
                continue;
            }
            if (key == QLatin1String("rate_func")) {
                clip.rateFunc = literal;
                continue;
            }
            // Rotate's angle arrives in radians as "90 * DEGREES".
            literal.remove(QStringLiteral(" * DEGREES"));

            for (const catalog::ParamSpec &param : spec->params) {
                if (param.id != key)
                    continue;
                const QVariant value = valueFor(param, literal);
                if (value.isValid())
                    clip.params.insert(param.id, value);
            }
        }

        parsed.addClip(clip);
        while (parsed.timeline.tracks.size() <= lane) {
            Track extra;
            extra.name = QStringLiteral("Track %1").arg(parsed.timeline.tracks.size() + 1);
            parsed.timeline.tracks.append(extra);
        }
        return offset + clip.duration;
    };

    for (const LogicalLine &logical : lines) {
        const QString line = logical.text;
        const int number = logical.number - 1;

        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1String("from manim")) || line.startsWith(QLatin1String("import ")))
            continue;
        if (line.startsWith(QLatin1String("def construct")))
            continue;

        const QRegularExpressionMatch classMatch = classPattern.match(line);
        if (classMatch.hasMatch()) {
            parsed.sceneClassName = classMatch.captured(1);
            sawClass = true;
            continue;
        }

        const QRegularExpressionMatch soundMatch = soundPattern.match(line);
        if (soundMatch.hasMatch()) {
            const QStringList arguments = splitArguments(soundMatch.captured(1));
            if (arguments.isEmpty())
                continue;
            AudioClip clip;
            clip.id = ClipId(parsed.timeline.audio.size() + 1);
            clip.asset = unquote(arguments.first());
            if (clip.asset.startsWith(QLatin1String("assets/")))
                clip.asset = clip.asset.mid(7);
            for (const QString &argument : arguments.mid(1)) {
                if (argument.startsWith(QLatin1String("time_offset=")))
                    clip.start = argument.mid(12).toDouble();
                else if (argument.startsWith(QLatin1String("gain=")))
                    clip.gain = argument.mid(5).toDouble();
            }
            parsed.timeline.audio.append(clip);
            continue;
        }

        const QRegularExpressionMatch waitMatch = waitPattern.match(line);
        if (waitMatch.hasMatch()) {
            cursor += waitMatch.captured(1).isEmpty() ? 1.0 : waitMatch.captured(1).toDouble();
            continue;
        }

        if (addPattern.match(line).hasMatch())
            continue;   // adding is implied by an object with no entrance

        const QRegularExpressionMatch playMatch = playPattern.match(line);
        if (playMatch.hasMatch()) {
            double longest = 0.0;
            int lane = 0;
            for (const QString &expression : splitArguments(playMatch.captured(1)))
                longest = qMax(longest, readAnimation(expression, cursor, lane++));
            cursor += longest;
            continue;
        }

        const QRegularExpressionMatch assignMatch = assignPattern.match(line);
        if (assignMatch.hasMatch()) {
            const QString variable = assignMatch.captured(1);
            const QString className = assignMatch.captured(2);

            if (className == QLatin1String("VGroup")) {
                SceneObject group;
                group.type = QStringLiteral("manim.VGroup");
                group.name = variable;
                group.params = catalog::defaultParams(*mobjectByPythonName(QStringLiteral("VGroup")));
                const ObjectId id = parsed.addObject(group);
                variables.insert(variable, id);

                for (const QString &member : splitArguments(assignMatch.captured(3))) {
                    if (SceneObject *child = parsed.findObject(variables.value(member, kInvalidObjectId)))
                        child->parentId = id;
                }
                continue;
            }

            const catalog::MobjectSpec *spec = mobjectByPythonName(className);
            if (!spec) {
                result.unrecognised.append(
                    QStringLiteral("line %1: %2").arg(number + 1).arg(line));
                continue;
            }

            SceneObject object;
            object.type = spec->id;
            object.name = variable;
            object.params = catalog::defaultParams(*spec);

            const QStringList arguments = splitArguments(assignMatch.captured(3));
            for (const QString &argument : arguments) {
                const int equals = int(argument.indexOf(QLatin1Char('=')));
                if (equals < 0) {
                    // Text and formulae take their content first, unnamed.
                    for (const catalog::ParamSpec &param : spec->params) {
                        if (param.id == QLatin1String("text") || param.id == QLatin1String("tex")) {
                            object.params.insert(param.id, unquote(argument));
                            break;
                        }
                    }
                    continue;
                }
                const QString key = argument.left(equals).trimmed();
                const QString literal = argument.mid(equals + 1).trimmed();
                for (const catalog::ParamSpec &param : spec->params) {
                    if (param.id != key)
                        continue;
                    const QVariant value = valueFor(param, literal);
                    if (value.isValid())
                        object.params.insert(param.id, value);
                }
            }

            variables.insert(variable, parsed.addObject(object));
            continue;
        }

        const QRegularExpressionMatch methodMatch = methodPattern.match(line);
        if (methodMatch.hasMatch()) {
            const QString variable = methodMatch.captured(1);
            const QString method = methodMatch.captured(2);
            const QString argument = methodMatch.captured(3).trimmed();

            SceneObject *object = parsed.findObject(variables.value(variable, kInvalidObjectId));
            if (!object) {
                result.unrecognised.append(
                    QStringLiteral("line %1: %2").arg(number + 1).arg(line));
                continue;
            }

            if (method == QLatin1String("move_to") || method == QLatin1String("shift")) {
                QPointF point;
                if (parsePoint(argument, &point))
                    object->params.insert(QStringLiteral("position"), point);
                continue;
            }
            if (method == QLatin1String("rotate")) {
                QString value = argument;
                value.remove(QStringLiteral(" * DEGREES"));
                object->params.insert(QStringLiteral("rotation"), value.toDouble());
                continue;
            }
            if (method == QLatin1String("scale")) {
                object->params.insert(QStringLiteral("scale"), argument.toDouble());
                continue;
            }

            result.unrecognised.append(QStringLiteral("line %1: %2").arg(number + 1).arg(line));
            continue;
        }

        result.unrecognised.append(QStringLiteral("line %1: %2").arg(number + 1).arg(line));
    }

    if (!sawClass) {
        result.error = QStringLiteral("No Scene subclass was found in this file.");
        return result;
    }

    // Python says nothing about rows, so overlapping animations are stacked
    // the same way a project from before rows existed is.
    parsed.timeline.packRows();

    parsed.timeline.duration = qMax(document->timeline.duration, cursor);
    *document = std::move(parsed);

    result.complete = result.unrecognised.isEmpty();
    return result;
}

} // namespace mn::codeparser
