#include "CodeGenerator.h"

#include "Catalog.h"
#include "Document.h"
#include "Project.h"

#include <QColor>
#include <QPointF>
#include <QRegularExpression>
#include <algorithm>

namespace mn::codegen {
namespace {

constexpr double kEpsilon = 1e-6;

QString indent(int level)
{
    return QString(level * 4, QLatin1Char(' '));
}

/// A Python identifier derived from the object's name, unique within the file.
QString variableName(const SceneObject &object, QSet<QString> &taken)
{
    QString base = project::pythonModuleName(object.name);
    if (base.isEmpty())
        base = QStringLiteral("mobject");

    QString candidate = base;
    for (int suffix = 2; taken.contains(candidate); ++suffix)
        candidate = QStringLiteral("%1_%2").arg(base).arg(suffix);

    taken.insert(candidate);
    return candidate;
}

QString quote(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1String("\\"), QLatin1String("\\\\"));
    escaped.replace(QLatin1String("\""), QLatin1String("\\\""));
    escaped.replace(QLatin1String("\n"), QLatin1String("\\n"));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString number(double value)
{
    if (qAbs(value - std::round(value)) < kEpsilon)
        return QString::number(qint64(std::round(value)));
    return QString::number(value, 'g', 6);
}

QString colorLiteral(const QColor &colour)
{
    return quote(colour.name(QColor::HexRgb).toUpper());
}

QString pointLiteral(const QPointF &point)
{
    return QStringLiteral("[%1, %2, 0]").arg(number(point.x()), number(point.y()));
}

bool sameValue(const QVariant &a, const QVariant &b)
{
    if (a.typeId() == QMetaType::QColor || b.typeId() == QMetaType::QColor)
        return a.value<QColor>() == b.value<QColor>();
    if (a.typeId() == QMetaType::QPointF || b.typeId() == QMetaType::QPointF)
        return a.toPointF() == b.toPointF();

    // Strings are compared as strings. QString reports that it can convert to
    // double, and every non-numeric string converts to zero, so comparing
    // numerically made every pair of words look identical.
    if (a.typeId() == QMetaType::QString || b.typeId() == QMetaType::QString)
        return a.toString() == b.toString();

    if (a.canConvert<double>() && b.canConvert<double>())
        return qAbs(a.toDouble() - b.toDouble()) < kEpsilon;
    return a == b;
}

QString valueLiteral(const catalog::ParamSpec &spec, const QVariant &value)
{
    switch (spec.type) {
    case catalog::ParamType::Number:
        return number(value.toDouble());
    case catalog::ParamType::Integer:
        return QString::number(value.toInt());
    case catalog::ParamType::Boolean:
        return value.toBool() ? QStringLiteral("True") : QStringLiteral("False");
    case catalog::ParamType::Color:
        return colorLiteral(value.value<QColor>());
    case catalog::ParamType::Point:
        return pointLiteral(value.toPointF());
    case catalog::ParamType::Text:
    case catalog::ParamType::Choice:
        return quote(value.toString());
    }
    return QStringLiteral("None");
}

/// Parameters that describe where a mobject is rather than what it is; they
/// become method calls after construction, as they would be written by hand.
bool isPlacement(const QString &id)
{
    return id == QLatin1String("position") || id == QLatin1String("rotation")
           || id == QLatin1String("scale");
}

QString constructorFor(const SceneObject &object, const catalog::MobjectSpec &spec)
{
    QStringList arguments;
    for (const catalog::ParamSpec &param : spec.params) {
        if (isPlacement(param.id))
            continue;

        const QVariant value = catalog::paramOr(object.params, spec.params, param.id);
        if (!param.emitWhenDefault && sameValue(value, param.defaultValue))
            continue;

        // Manim's own constructors take the text first, unnamed.
        if (param.id == QLatin1String("text") || param.id == QLatin1String("tex")) {
            arguments.prepend(valueLiteral(param, value));
            continue;
        }
        arguments.append(QStringLiteral("%1=%2").arg(param.id, valueLiteral(param, value)));
    }

    return QStringLiteral("%1(%2)").arg(spec.pythonName, arguments.join(QStringLiteral(", ")));
}

QStringList placementFor(const SceneObject &object, const catalog::MobjectSpec &spec,
                         const QString &variable)
{
    QStringList lines;

    const QPointF position =
        catalog::paramOr(object.params, spec.params, QStringLiteral("position")).toPointF();

    // A line and an arrow already say where they are through their endpoints,
    // so their position shifts them. Everything else is centred on it.
    const bool isOffset = spec.shape == catalog::ShapeKind::Line
                          || spec.shape == catalog::ShapeKind::Arrow;

    if (isOffset) {
        if (!position.isNull())
            lines.append(QStringLiteral("%1.shift(%2)").arg(variable, pointLiteral(position)));
    } else {
        // Emitted even at the origin. move_to centres the bounding box, and for
        // a shape whose natural centre is not its box's centre — a triangle,
        // say — leaving it out would place it somewhere the canvas does not.
        lines.append(QStringLiteral("%1.move_to(%2)").arg(variable, pointLiteral(position)));
    }

    const double rotation =
        catalog::paramOr(object.params, spec.params, QStringLiteral("rotation")).toDouble();
    if (qAbs(rotation) > kEpsilon)
        lines.append(QStringLiteral("%1.rotate(%2 * DEGREES)").arg(variable, number(rotation)));

    const double scale =
        catalog::paramOr(object.params, spec.params, QStringLiteral("scale")).toDouble();
    if (qAbs(scale - 1.0) > kEpsilon)
        lines.append(QStringLiteral("%1.scale(%2)").arg(variable, number(scale)));

    return lines;
}

/// The Python expression for one clip's animation.
QString animationExpression(const Clip &clip, const catalog::AnimationSpec &spec,
                            const QString &variable)
{
    // Timing kwargs, shared by both shapes an animation can take.
    QStringList timing;
    timing.append(QStringLiteral("run_time=%1").arg(number(clip.duration)));
    if (!clip.rateFunc.isEmpty() && clip.rateFunc != QLatin1String("smooth"))
        timing.append(QStringLiteral("rate_func=%1").arg(clip.rateFunc));

    // Changes to a mobject are written as `.animate`, which is how they are
    // written by hand. It takes its own timing, so several can share one play.
    const QString animate = QStringLiteral("%1.animate(%2)").arg(variable, timing.join(QStringLiteral(", ")));

    switch (spec.effect) {
    case catalog::Effect::Code:
        // Not an animation at all: a block of Python, placed in the sequence.
        return {};

    case catalog::Effect::MoveTo:
        return animate
               + QStringLiteral(".move_to(%1)")
                     .arg(pointLiteral(
                         catalog::paramOr(clip.params, spec.params, QStringLiteral("to")).toPointF()));
    case catalog::Effect::Fade:
        return animate
               + QStringLiteral(".set_opacity(%1)")
                     .arg(number(catalog::paramOr(clip.params, spec.params,
                                                  QStringLiteral("to")).toDouble()));

    case catalog::Effect::Shift:
        return animate
               + QStringLiteral(".shift(%1)")
                     .arg(pointLiteral(
                         catalog::paramOr(clip.params, spec.params, QStringLiteral("by")).toPointF()));
    case catalog::Effect::Scale:
        return animate
               + QStringLiteral(".scale(%1)")
                     .arg(number(catalog::paramOr(clip.params, spec.params,
                                                  QStringLiteral("factor")).toDouble()));
    case catalog::Effect::Recolor:
        return animate
               + QStringLiteral(".set_color(%1)")
                     .arg(colorLiteral(catalog::paramOr(clip.params, spec.params,
                                                        QStringLiteral("to")).value<QColor>()));
    default:
        break;
    }

    QStringList arguments;
    // Wait takes no mobject; everything else animates one.
    if (spec.effect != catalog::Effect::Wait)
        arguments.append(variable);

    for (const catalog::ParamSpec &param : spec.params) {
        const QVariant value = catalog::paramOr(clip.params, spec.params, param.id);
        if (sameValue(value, param.defaultValue))
            continue;

        // Manim's Rotate takes an angle in radians.
        if (spec.effect == catalog::Effect::Rotate && param.id == QLatin1String("angle")) {
            arguments.append(QStringLiteral("angle=%1 * DEGREES").arg(number(value.toDouble())));
            continue;
        }
        arguments.append(QStringLiteral("%1=%2").arg(param.id, valueLiteral(param, value)));
    }
    arguments += timing;

    return QStringLiteral("%1(%2)").arg(spec.pythonName, arguments.join(QStringLiteral(", ")));
}

/// A group of clips that overlap, directly or through a chain of neighbours.
struct Cluster
{
    double start = 0.0;
    double end = 0.0;
    QVector<const Clip *> clips;
};

QVector<Cluster> clusterClips(const QVector<Clip> &clips)
{
    QVector<const Clip *> sorted;
    for (const Clip &clip : clips)
        sorted.append(&clip);
    std::sort(sorted.begin(), sorted.end(),
              [](const Clip *a, const Clip *b) { return a->start < b->start; });

    QVector<Cluster> clusters;
    for (const Clip *clip : std::as_const(sorted)) {
        if (!clusters.isEmpty() && clip->start < clusters.last().end - kEpsilon) {
            clusters.last().clips.append(clip);
            clusters.last().end = qMax(clusters.last().end, clip->end());
            continue;
        }
        Cluster cluster;
        cluster.start = clip->start;
        cluster.end = clip->end();
        cluster.clips.append(clip);
        clusters.append(cluster);
    }
    return clusters;
}

} // namespace

QString constructBody(const Document &document, int indentLevel)
{
    const QString pad = indent(indentLevel);
    QStringList lines;

    // ------------------------------------------------------------ objects ---
    QSet<QString> taken;
    QHash<ObjectId, QString> variables;

    // A group has to be written after the things it holds, so order by depth.
    QVector<const SceneObject *> ordered;
    for (const SceneObject &object : document.objects)
        ordered.append(&object);

    auto depthOf = [&document](const SceneObject *object) {
        int depth = 0;
        ObjectId ancestor = object->parentId;
        while (ancestor != kInvalidObjectId && depth < 64) {
            const SceneObject *parent = document.findObject(ancestor);
            if (!parent)
                break;
            ++depth;
            ancestor = parent->parentId;
        }
        return depth;
    };
    std::stable_sort(ordered.begin(), ordered.end(),
                     [&depthOf](const SceneObject *a, const SceneObject *b) {
                         return depthOf(a) > depthOf(b);
                     });

    for (const SceneObject *pointer : std::as_const(ordered)) {
        const SceneObject &object = *pointer;
        const catalog::MobjectSpec *spec = catalog::findMobject(object.type);
        if (!spec)
            continue;

        const QString variable = variableName(object, taken);
        variables.insert(object.id, variable);

        if (spec->shape == catalog::ShapeKind::Group) {
            QStringList members;
            for (const SceneObject &child : document.objects) {
                if (child.parentId == object.id && variables.contains(child.id))
                    members.append(variables.value(child.id));
            }
            lines.append(pad + QStringLiteral("%1 = VGroup(%2)")
                                   .arg(variable, members.join(QStringLiteral(", "))));
            for (const QString &placement : placementFor(object, *spec, variable))
                lines.append(pad + placement);
            continue;
        }

        if (spec->shape == catalog::ShapeKind::Custom) {
            // Written by hand, emitted exactly as written.
            const QString expression =
                catalog::paramOr(object.params, spec->params, QStringLiteral("expression")).toString();
            lines.append(pad + QStringLiteral("%1 = %2").arg(variable, expression));
            for (const QString &placement : placementFor(object, *spec, variable))
                lines.append(pad + placement);
            continue;
        }

        lines.append(pad + QStringLiteral("%1 = %2").arg(variable, constructorFor(object, *spec)));
        for (const QString &placement : placementFor(object, *spec, variable))
            lines.append(pad + placement);
        if (!object.rawPython.isEmpty()) {
            for (const QString &raw : object.rawPython.split(QLatin1Char('\n')))
                lines.append(pad + raw);
        }
    }

    // Anything never animated on has to be added, or it never appears. A child
    // is added by its group, so only the outermost objects are listed.
    QStringList staticObjects;
    for (const SceneObject &object : document.objects) {
        if (!variables.contains(object.id) || !object.visible)
            continue;
        if (object.parentId != kInvalidObjectId)
            continue;
        bool hasEntrance = false;
        for (const Clip &clip : document.timeline.clips) {
            const catalog::AnimationSpec *spec = catalog::findAnimation(clip.type);
            if (clip.objectId == object.id && spec && spec->isEntrance) {
                hasEntrance = true;
                break;
            }
        }
        if (!hasEntrance)
            staticObjects.append(variables.value(object.id));
    }
    if (!staticObjects.isEmpty()) {
        lines.append(QString());
        lines.append(pad + QStringLiteral("self.add(%1)").arg(staticObjects.join(QStringLiteral(", "))));
    }

    // -------------------------------------------------------------- audio ---
    // Manim takes sounds one at a time with an offset from the start of the
    // scene, so the whole audio timeline is written out before anything plays.
    if (!document.timeline.audio.isEmpty()) {
        lines.append(QString());
        for (const AudioClip &clip : document.timeline.audio) {
            if (clip.asset.isEmpty())
                continue;

            QStringList arguments{quote(QStringLiteral("assets/") + clip.asset)};
            if (qAbs(clip.start) > kEpsilon)
                arguments.append(QStringLiteral("time_offset=%1").arg(number(clip.start)));
            if (qAbs(clip.gain) > kEpsilon)
                arguments.append(QStringLiteral("gain=%1").arg(number(clip.gain)));

            lines.append(pad + QStringLiteral("self.add_sound(%1)")
                                   .arg(arguments.join(QStringLiteral(", "))));
        }
    }

    // ----------------------------------------------------------- timeline ---
    const QVector<Cluster> clusters = clusterClips(document.timeline.clips);

    double cursor = 0.0;
    for (const Cluster &cluster : clusters) {
        const double gap = cluster.start - cursor;
        if (gap > kEpsilon) {
            lines.append(QString());
            lines.append(pad + QStringLiteral("self.wait(%1)").arg(number(gap)));
        }

        // A Python block is not an animation; it is written out on its own,
        // in the sequence, with its target substituted in.
        QStringList blocks;
        for (const Clip *clip : cluster.clips) {
            const catalog::AnimationSpec *spec = catalog::findAnimation(clip->type);
            if (!spec || spec->effect != catalog::Effect::Code)
                continue;

            QString body =
                catalog::paramOr(clip->params, spec->params, QStringLiteral("body")).toString();
            if (body.trimmed().isEmpty())
                continue;

            // TARGET stands for whatever the block is attached to, so a block
            // can be written once and pointed at different objects.
            body.replace(QLatin1String("TARGET"), variables.value(clip->objectId,
                                                                  QStringLiteral("self")));
            for (const QString &line : body.split(QLatin1Char('\n')))
                blocks.append(pad + line);
        }

        QStringList expressions;
        for (const Clip *clip : cluster.clips) {
            const catalog::AnimationSpec *spec = catalog::findAnimation(clip->type);
            if (!spec || spec->effect == catalog::Effect::Code)
                continue;
            if (!variables.contains(clip->objectId))
                continue;

            QString expression;
            if (!clip->rawPython.isEmpty()) {
                expression = clip->rawPython;
            } else {
                expression = animationExpression(*clip, *spec, variables.value(clip->objectId));
            }
            if (expression.isEmpty())
                continue;

            // A clip starting after its cluster does so exactly, by waiting
            // first. This is how an arbitrary offset survives into Manim.
            const double offset = clip->start - cluster.start;
            if (offset > kEpsilon) {
                expression = QStringLiteral("Succession(Wait(%1), %2)")
                                 .arg(number(offset), expression);
            }
            expressions.append(expression);
        }

        if (!blocks.isEmpty()) {
            lines.append(QString());
            lines += blocks;
        }

        if (expressions.isEmpty()) {
            cursor = qMax(cursor, cluster.end);
            continue;
        }

        lines.append(QString());
        if (expressions.size() == 1) {
            lines.append(pad + QStringLiteral("self.play(%1)").arg(expressions.first()));
        } else {
            lines.append(pad + QStringLiteral("self.play("));
            for (int i = 0; i < expressions.size(); ++i) {
                lines.append(indent(indentLevel + 1) + expressions.at(i)
                             + (i + 1 < expressions.size() ? QStringLiteral(",") : QString()));
            }
            lines.append(pad + QStringLiteral(")"));
        }

        cursor = cluster.end;
    }

    const double tail = document.timeline.duration - cursor;
    if (tail > kEpsilon) {
        lines.append(QString());
        lines.append(pad + QStringLiteral("self.wait(%1)").arg(number(tail)));
    }

    if (lines.isEmpty())
        lines.append(pad + QStringLiteral("pass"));

    return lines.join(QStringLiteral("\n"));
}

QString generate(const Document &document)
{
    const QString sceneClass =
        document.sceneClassName.isEmpty() ? QStringLiteral("MainScene") : document.sceneClassName;

    QStringList lines;
    lines.append(QStringLiteral("from manim import *"));
    lines.append(QString());
    lines.append(QString());
    lines.append(QStringLiteral("class %1(Scene):").arg(sceneClass));
    lines.append(indent(1) + QStringLiteral("def construct(self):"));
    lines.append(constructBody(document, 2));
    lines.append(QString());

    return lines.join(QStringLiteral("\n"));
}

} // namespace mn::codegen
