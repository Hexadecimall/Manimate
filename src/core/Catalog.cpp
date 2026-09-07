#include "Catalog.h"

#include <QPointF>

namespace mn::catalog {
namespace {

/// Manim's own defaults, so a shape placed here looks like the same shape
/// rendered there.
const QColor kManimWhite(0xFF, 0xFF, 0xFF);
const QColor kManimBlue(0x58, 0xC4, 0xDD);
const QColor kManimYellow(0xFF, 0xFF, 0x00);

ParamSpec number(const QString &id, const QString &label, double value, double min = -100.0,
                 double max = 100.0, double step = 0.1)
{
    ParamSpec spec;
    spec.id = id;
    spec.label = label;
    spec.type = ParamType::Number;
    spec.defaultValue = value;
    spec.minimum = min;
    spec.maximum = max;
    spec.step = step;
    return spec;
}

ParamSpec integer(const QString &id, const QString &label, int value, int min, int max)
{
    ParamSpec spec;
    spec.id = id;
    spec.label = label;
    spec.type = ParamType::Integer;
    spec.defaultValue = value;
    spec.minimum = min;
    spec.maximum = max;
    spec.step = 1;
    return spec;
}

ParamSpec color(const QString &id, const QString &label, const QColor &value)
{
    ParamSpec spec;
    spec.id = id;
    spec.label = label;
    spec.type = ParamType::Color;
    spec.defaultValue = value;
    return spec;
}

ParamSpec point(const QString &id, const QString &label, QPointF value)
{
    ParamSpec spec;
    spec.id = id;
    spec.label = label;
    spec.type = ParamType::Point;
    spec.defaultValue = value;
    return spec;
}

ParamSpec text(const QString &id, const QString &label, const QString &value)
{
    ParamSpec spec;
    spec.id = id;
    spec.label = label;
    spec.type = ParamType::Text;
    spec.defaultValue = value;
    return spec;
}

ParamSpec boolean(const QString &id, const QString &label, bool value)
{
    ParamSpec spec;
    spec.id = id;
    spec.label = label;
    spec.type = ParamType::Boolean;
    spec.defaultValue = value;
    return spec;
}

/// Parameters every mobject has: where it sits, and how it is painted.
QVector<ParamSpec> commonParams(const QColor &stroke = kManimWhite, double strokeWidth = 4.0,
                                double fillOpacity = 0.0)
{
    return {
        point(QStringLiteral("position"), QStringLiteral("Position"), QPointF(0, 0)),
        color(QStringLiteral("color"), QStringLiteral("Colour"), stroke),
        number(QStringLiteral("stroke_width"), QStringLiteral("Stroke"), strokeWidth, 0, 40, 0.5),
        number(QStringLiteral("fill_opacity"), QStringLiteral("Fill"), fillOpacity, 0, 1, 0.05),
        number(QStringLiteral("rotation"), QStringLiteral("Rotation"), 0, -360, 360, 1),
        number(QStringLiteral("scale"), QStringLiteral("Scale"), 1.0, 0.01, 20, 0.05),
    };
}

QVector<ParamSpec> operator+(QVector<ParamSpec> lhs, const QVector<ParamSpec> &rhs)
{
    lhs.append(rhs);
    return lhs;
}

MobjectSpec makeMobject(const QString &pythonName, const QString &display, const QString &category,
                        ShapeKind shape, const QVector<ParamSpec> &params)
{
    MobjectSpec spec;
    spec.id = QStringLiteral("manim.") + pythonName;
    spec.pythonName = pythonName;
    spec.displayName = display;
    spec.category = category;
    spec.shape = shape;
    spec.params = params;
    spec.defaultObjectName = display;
    return spec;
}

AnimationSpec makeAnimation(const QString &pythonName, const QString &display,
                            const QString &category, Effect effect,
                            const QVector<ParamSpec> &params = {})
{
    AnimationSpec spec;
    spec.id = QStringLiteral("manim.") + pythonName;
    spec.pythonName = pythonName;
    spec.displayName = display;
    spec.category = category;
    spec.effect = effect;
    spec.params = params;
    return spec;
}

} // namespace

const QVector<MobjectSpec> &mobjects()
{
    static const QVector<MobjectSpec> specs = [] {
        QVector<MobjectSpec> result;

        result.append(makeMobject(
            QStringLiteral("Circle"), QStringLiteral("Circle"), QStringLiteral("Shapes"),
            ShapeKind::Circle,
            QVector<ParamSpec>{number(QStringLiteral("radius"), QStringLiteral("Radius"), 1.0, 0.01, 20)}
                + commonParams()));

        result.append(makeMobject(
            QStringLiteral("Ellipse"), QStringLiteral("Ellipse"), QStringLiteral("Shapes"),
            ShapeKind::Ellipse,
            QVector<ParamSpec>{number(QStringLiteral("width"), QStringLiteral("Width"), 2.0, 0.01, 30),
                               number(QStringLiteral("height"), QStringLiteral("Height"), 1.0, 0.01, 30)}
                + commonParams()));

        result.append(makeMobject(
            QStringLiteral("Square"), QStringLiteral("Square"), QStringLiteral("Shapes"),
            ShapeKind::Rectangle,
            QVector<ParamSpec>{
                number(QStringLiteral("side_length"), QStringLiteral("Side"), 2.0, 0.01, 30)}
                + commonParams()));

        result.append(makeMobject(
            QStringLiteral("Rectangle"), QStringLiteral("Rectangle"), QStringLiteral("Shapes"),
            ShapeKind::Rectangle,
            QVector<ParamSpec>{number(QStringLiteral("width"), QStringLiteral("Width"), 4.0, 0.01, 30),
                               number(QStringLiteral("height"), QStringLiteral("Height"), 2.0, 0.01, 30)}
                + commonParams()));

        result.append(makeMobject(
            QStringLiteral("RoundedRectangle"), QStringLiteral("Rounded rectangle"),
            QStringLiteral("Shapes"), ShapeKind::RoundedRectangle,
            QVector<ParamSpec>{number(QStringLiteral("width"), QStringLiteral("Width"), 4.0, 0.01, 30),
                               number(QStringLiteral("height"), QStringLiteral("Height"), 2.0, 0.01, 30),
                               number(QStringLiteral("corner_radius"), QStringLiteral("Corner"), 0.5,
                                      0.0, 10)}
                + commonParams()));

        result.append(makeMobject(
            QStringLiteral("Triangle"), QStringLiteral("Triangle"), QStringLiteral("Shapes"),
            ShapeKind::RegularPolygon,
            QVector<ParamSpec>{integer(QStringLiteral("n"), QStringLiteral("Sides"), 3, 3, 3),
                               number(QStringLiteral("radius"), QStringLiteral("Radius"), 1.0, 0.01, 20)}
                + commonParams()));

        result.append(makeMobject(
            QStringLiteral("RegularPolygon"), QStringLiteral("Polygon"), QStringLiteral("Shapes"),
            ShapeKind::RegularPolygon,
            QVector<ParamSpec>{integer(QStringLiteral("n"), QStringLiteral("Sides"), 6, 3, 60),
                               number(QStringLiteral("radius"), QStringLiteral("Radius"), 1.0, 0.01, 20)}
                + commonParams()));

        result.append(makeMobject(
            QStringLiteral("Star"), QStringLiteral("Star"), QStringLiteral("Shapes"), ShapeKind::Star,
            QVector<ParamSpec>{integer(QStringLiteral("n"), QStringLiteral("Points"), 5, 3, 40),
                               number(QStringLiteral("outer_radius"), QStringLiteral("Outer"), 1.0, 0.01, 20),
                               number(QStringLiteral("inner_radius"), QStringLiteral("Inner"), 0.5, 0.01, 20)}
                + commonParams()));

        result.append(makeMobject(
            QStringLiteral("Dot"), QStringLiteral("Dot"), QStringLiteral("Shapes"), ShapeKind::Dot,
            QVector<ParamSpec>{number(QStringLiteral("radius"), QStringLiteral("Radius"), 0.08, 0.01, 5, 0.01)}
                + commonParams(kManimWhite, 0.0, 1.0)));

        result.append(makeMobject(
            QStringLiteral("Line"), QStringLiteral("Line"), QStringLiteral("Shapes"), ShapeKind::Line,
            QVector<ParamSpec>{point(QStringLiteral("start"), QStringLiteral("Start"), QPointF(-1, 0)),
                               point(QStringLiteral("end"), QStringLiteral("End"), QPointF(1, 0))}
                + commonParams(kManimWhite, 4.0)));

        result.append(makeMobject(
            QStringLiteral("Arrow"), QStringLiteral("Arrow"), QStringLiteral("Shapes"), ShapeKind::Arrow,
            QVector<ParamSpec>{point(QStringLiteral("start"), QStringLiteral("Start"), QPointF(-1, 0)),
                               point(QStringLiteral("end"), QStringLiteral("End"), QPointF(1, 0)),
                               number(QStringLiteral("tip_length"), QStringLiteral("Tip"), 0.25, 0.05, 3, 0.05)}
                + commonParams(kManimWhite, 4.0)));

        result.append(makeMobject(
            QStringLiteral("Text"), QStringLiteral("Text"), QStringLiteral("Text"), ShapeKind::Text,
            QVector<ParamSpec>{text(QStringLiteral("text"), QStringLiteral("Text"), QStringLiteral("Text")),
                               number(QStringLiteral("font_size"), QStringLiteral("Size"), 48, 4, 400, 1),
                               boolean(QStringLiteral("bold"), QStringLiteral("Bold"), false),
                               boolean(QStringLiteral("italic"), QStringLiteral("Italic"), false)}
                + commonParams(kManimWhite, 0.0, 1.0)));

        result.append(makeMobject(
            QStringLiteral("MathTex"), QStringLiteral("Formula"), QStringLiteral("Text"),
            ShapeKind::MathText,
            QVector<ParamSpec>{
                text(QStringLiteral("tex"), QStringLiteral("LaTeX"), QStringLiteral("x^2 + y^2 = r^2")),
                number(QStringLiteral("font_size"), QStringLiteral("Size"), 48, 4, 400, 1)}
                + commonParams(kManimWhite, 0.0, 1.0)));

        result.append(makeMobject(
            QStringLiteral("NumberPlane"), QStringLiteral("Grid"), QStringLiteral("Graphs"),
            ShapeKind::NumberPlane,
            QVector<ParamSpec>{number(QStringLiteral("x_range"), QStringLiteral("X range"), 7.0, 1, 30, 1),
                               number(QStringLiteral("y_range"), QStringLiteral("Y range"), 4.0, 1, 30, 1)}
                + commonParams(QColor(0x29, 0xAB, 0xCA), 2.0)));

        result.append(makeMobject(
            QStringLiteral("Axes"), QStringLiteral("Axes"), QStringLiteral("Graphs"), ShapeKind::Axes,
            QVector<ParamSpec>{number(QStringLiteral("x_range"), QStringLiteral("X range"), 6.0, 1, 30, 1),
                               number(QStringLiteral("y_range"), QStringLiteral("Y range"), 3.0, 1, 30, 1),
                               boolean(QStringLiteral("tips"), QStringLiteral("Arrow tips"), true)}
                + commonParams()));

        return result;
    }();
    return specs;
}

const QVector<AnimationSpec> &animations()
{
    static const QVector<AnimationSpec> specs = [] {
        QVector<AnimationSpec> result;

        auto entrance = [](AnimationSpec spec) {
            spec.isEntrance = true;
            return spec;
        };

        result.append(entrance(makeAnimation(QStringLiteral("Create"), QStringLiteral("Create"),
                                             QStringLiteral("Entrance"), Effect::Draw)));
        result.append(entrance(makeAnimation(QStringLiteral("Write"), QStringLiteral("Write"),
                                             QStringLiteral("Entrance"), Effect::Draw)));
        result.append(entrance(makeAnimation(QStringLiteral("DrawBorderThenFill"),
                                             QStringLiteral("Draw border then fill"),
                                             QStringLiteral("Entrance"), Effect::Draw)));
        result.append(entrance(makeAnimation(
            QStringLiteral("FadeIn"), QStringLiteral("Fade in"), QStringLiteral("Entrance"),
            Effect::FadeIn,
            {point(QStringLiteral("shift"), QStringLiteral("Shift from"), QPointF(0, 0))})));
        result.append(entrance(makeAnimation(QStringLiteral("GrowFromCenter"),
                                             QStringLiteral("Grow from centre"),
                                             QStringLiteral("Entrance"), Effect::Grow)));

        AnimationSpec fadeOut = makeAnimation(
            QStringLiteral("FadeOut"), QStringLiteral("Fade out"), QStringLiteral("Exit"),
            Effect::FadeOut,
            {point(QStringLiteral("shift"), QStringLiteral("Shift to"), QPointF(0, 0))});
        fadeOut.isExit = true;
        result.append(fadeOut);

        AnimationSpec uncreate = makeAnimation(QStringLiteral("Uncreate"), QStringLiteral("Uncreate"),
                                               QStringLiteral("Exit"), Effect::Draw);
        uncreate.isExit = true;
        result.append(uncreate);

        AnimationSpec shrink = makeAnimation(QStringLiteral("ShrinkToCenter"),
                                             QStringLiteral("Shrink to centre"),
                                             QStringLiteral("Exit"), Effect::Grow);
        shrink.isExit = true;
        result.append(shrink);

        result.append(makeAnimation(
            QStringLiteral("Shift"), QStringLiteral("Shift"), QStringLiteral("Motion"), Effect::Shift,
            {point(QStringLiteral("by"), QStringLiteral("By"), QPointF(1, 0))}));
        result.append(makeAnimation(
            QStringLiteral("Rotate"), QStringLiteral("Rotate"), QStringLiteral("Motion"), Effect::Rotate,
            {number(QStringLiteral("angle"), QStringLiteral("Angle"), 90, -1440, 1440, 5)}));
        result.append(makeAnimation(
            QStringLiteral("ScaleBy"), QStringLiteral("Scale"), QStringLiteral("Motion"), Effect::Scale,
            {number(QStringLiteral("factor"), QStringLiteral("Factor"), 1.5, 0.01, 20, 0.05)}));
        result.append(makeAnimation(
            QStringLiteral("Recolor"), QStringLiteral("Change colour"), QStringLiteral("Motion"),
            Effect::Recolor, {color(QStringLiteral("to"), QStringLiteral("To"), kManimYellow)}));

        result.append(makeAnimation(QStringLiteral("Wait"), QStringLiteral("Wait"),
                                    QStringLiteral("Timing"), Effect::Wait));

        return result;
    }();
    return specs;
}

const MobjectSpec *findMobject(const QString &id)
{
    for (const MobjectSpec &spec : mobjects()) {
        if (spec.id == id)
            return &spec;
    }
    return nullptr;
}

const AnimationSpec *findAnimation(const QString &id)
{
    for (const AnimationSpec &spec : animations()) {
        if (spec.id == id)
            return &spec;
    }
    return nullptr;
}

QStringList mobjectCategories()
{
    QStringList result;
    for (const MobjectSpec &spec : mobjects()) {
        if (!result.contains(spec.category))
            result.append(spec.category);
    }
    return result;
}

QStringList animationCategories()
{
    QStringList result;
    for (const AnimationSpec &spec : animations()) {
        if (!result.contains(spec.category))
            result.append(spec.category);
    }
    return result;
}

namespace {

QVariantMap defaultsOf(const QVector<ParamSpec> &specs)
{
    QVariantMap result;
    for (const ParamSpec &spec : specs)
        result.insert(spec.id, spec.defaultValue);
    return result;
}

} // namespace

QVariantMap defaultParams(const MobjectSpec &spec)
{
    return defaultsOf(spec.params);
}

QVariantMap defaultParams(const AnimationSpec &spec)
{
    return defaultsOf(spec.params);
}

QVariant paramOr(const QVariantMap &params, const QVector<ParamSpec> &specs, const QString &id)
{
    const auto found = params.constFind(id);
    if (found != params.constEnd() && found->isValid())
        return *found;

    for (const ParamSpec &spec : specs) {
        if (spec.id == id)
            return spec.defaultValue;
    }
    return {};
}

} // namespace mn::catalog
