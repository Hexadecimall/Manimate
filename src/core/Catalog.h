#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

namespace mn {

/// What the editor knows about Manim's classes.
///
/// A catalog entry says three things: what parameters a class takes, how to
/// draw it on the canvas, and what Python it becomes. Simple classes are pure
/// data here; the ones whose drawing is real logic name a shape the renderer
/// implements in code. That split is deliberate — adding Ellipse should not
/// mean writing C++, but adding Axes should.
namespace catalog {

/// The kind of value a parameter holds, which decides how the inspector edits
/// it and how it is written into Python.
enum class ParamType {
    Number,
    Integer,
    Boolean,
    Color,
    Point,
    Text,
    Choice,
};

struct ParamSpec
{
    QString id;
    QString label;
    ParamType type = ParamType::Number;
    QVariant defaultValue;

    double minimum = -1000.0;
    double maximum = 1000.0;
    double step = 0.1;

    /// For ParamType::Choice.
    QStringList choices;

    /// Emitted as a keyword argument only when it differs from the default,
    /// so generated Python stays close to what a person would write.
    bool emitWhenDefault = false;
};

/// How the canvas draws a mobject. The renderer implements each of these; the
/// catalog only names one and maps the entry's parameters onto it.
enum class ShapeKind {
    Circle,
    Ellipse,
    Rectangle,
    RoundedRectangle,
    RegularPolygon,
    Star,
    Line,
    Arrow,
    Dot,
    Text,
    MathText,
    NumberPlane,
    Axes,
};

/// A class that can exist in a scene.
struct MobjectSpec
{
    QString id;            ///< "manim.Circle"
    QString pythonName;    ///< "Circle"
    QString displayName;   ///< "Circle"
    QString category;      ///< "Shapes", "Text", "Graphs"
    ShapeKind shape = ShapeKind::Circle;
    QVector<ParamSpec> params;

    /// Suggested name for a new instance, before deduplication.
    QString defaultObjectName;
};

/// What an animation does to a mobject, which is all the evaluator needs to
/// know to show it partway through.
enum class Effect {
    /// The mobject is drawn progressively into existence.
    Draw,
    /// Opacity rises from nothing.
    FadeIn,
    /// Opacity falls to nothing, and it is gone afterwards.
    FadeOut,
    /// Grows from a point to full size.
    Grow,
    /// Moves by an offset.
    Shift,
    /// Turns about its centre.
    Rotate,
    /// Changes size.
    Scale,
    /// Changes colour.
    Recolor,
    /// Nothing visible happens; time simply passes.
    Wait,
};

struct AnimationSpec
{
    QString id;           ///< "manim.Create"
    QString pythonName;   ///< "Create"
    QString displayName;
    QString category;     ///< "Entrance", "Motion", "Exit"
    Effect effect = Effect::Draw;
    QVector<ParamSpec> params;

    /// True when the mobject is on screen only after this animation begins.
    bool isEntrance = false;

    /// True when the mobject is gone once this animation ends.
    bool isExit = false;

    double defaultDuration = 1.0;
};

/// Everything the editor can place in a scene.
const QVector<MobjectSpec> &mobjects();
const QVector<AnimationSpec> &animations();

const MobjectSpec *findMobject(const QString &id);
const AnimationSpec *findAnimation(const QString &id);

/// Categories in the order the library should show them.
QStringList mobjectCategories();
QStringList animationCategories();

/// Every parameter at its default, ready to be stored on a new object.
QVariantMap defaultParams(const MobjectSpec &spec);
QVariantMap defaultParams(const AnimationSpec &spec);

/// Look up a parameter, falling back to the spec's default when the object
/// does not carry one.
QVariant paramOr(const QVariantMap &params, const QVector<ParamSpec> &specs, const QString &id);

} // namespace catalog
} // namespace mn
