#include "Catalog.h"

#include <QPointF>

#include <utility>

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
        number(QStringLiteral("z"), QStringLiteral("Height"), 0.0, -100, 100, 0.1),
        color(QStringLiteral("color"), QStringLiteral("Colour"), stroke),
        number(QStringLiteral("stroke_width"), QStringLiteral("Stroke"), strokeWidth, 0, 40, 0.5),
        number(QStringLiteral("fill_opacity"), QStringLiteral("Fill"), fillOpacity, 0, 1, 0.05),
        number(QStringLiteral("rotation"), QStringLiteral("Rotation"), 0, -360, 360, 1),
        number(QStringLiteral("scale"), QStringLiteral("Scale"), 1.0, 0.01, 20, 0.05),
    };
}

/// Own parameters first, then the ones every mobject shares.
QVector<ParamSpec> joined(const QVector<ParamSpec> &own, const QVector<ParamSpec> &common)
{
    QVector<ParamSpec> all = own;
    all.append(common);
    return all;
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

        auto add = [&result](const QString &pythonName, const QString &display,
                             const QString &category, ShapeKind shape,
                             const QVector<ParamSpec> &own,
                             const QVector<ParamSpec> &common = commonParams()) {
            result.append(makeMobject(pythonName, display, category, shape, joined(own, common)));
        };

        // ----------------------------------------------------------- shapes ---
        add(QStringLiteral("Circle"), QStringLiteral("Circle"), QStringLiteral("Shapes"),
            ShapeKind::Circle,
            {number(QStringLiteral("radius"), QStringLiteral("Radius"), 1.0, 0.01, 20)});

        add(QStringLiteral("Ellipse"), QStringLiteral("Ellipse"), QStringLiteral("Shapes"),
            ShapeKind::Ellipse,
            {number(QStringLiteral("width"), QStringLiteral("Width"), 2.0, 0.01, 30),
             number(QStringLiteral("height"), QStringLiteral("Height"), 1.0, 0.01, 30)});

        add(QStringLiteral("Square"), QStringLiteral("Square"), QStringLiteral("Shapes"),
            ShapeKind::Rectangle,
            {number(QStringLiteral("side_length"), QStringLiteral("Side"), 2.0, 0.01, 30)});

        add(QStringLiteral("Rectangle"), QStringLiteral("Rectangle"), QStringLiteral("Shapes"),
            ShapeKind::Rectangle,
            {number(QStringLiteral("width"), QStringLiteral("Width"), 4.0, 0.01, 30),
             number(QStringLiteral("height"), QStringLiteral("Height"), 2.0, 0.01, 30)});

        add(QStringLiteral("RoundedRectangle"), QStringLiteral("Rounded rectangle"),
            QStringLiteral("Shapes"), ShapeKind::RoundedRectangle,
            {number(QStringLiteral("width"), QStringLiteral("Width"), 4.0, 0.01, 30),
             number(QStringLiteral("height"), QStringLiteral("Height"), 2.0, 0.01, 30),
             number(QStringLiteral("corner_radius"), QStringLiteral("Corner"), 0.5, 0.0, 10)});

        add(QStringLiteral("Triangle"), QStringLiteral("Triangle"), QStringLiteral("Shapes"),
            ShapeKind::RegularPolygon,
            {integer(QStringLiteral("n"), QStringLiteral("Sides"), 3, 3, 3),
             number(QStringLiteral("radius"), QStringLiteral("Radius"), 1.0, 0.01, 20)});

        add(QStringLiteral("RegularPolygon"), QStringLiteral("Polygon"), QStringLiteral("Shapes"),
            ShapeKind::RegularPolygon,
            {integer(QStringLiteral("n"), QStringLiteral("Sides"), 6, 3, 60),
             number(QStringLiteral("radius"), QStringLiteral("Radius"), 1.0, 0.01, 20)});

        add(QStringLiteral("Star"), QStringLiteral("Star"), QStringLiteral("Shapes"),
            ShapeKind::Star,
            {integer(QStringLiteral("n"), QStringLiteral("Points"), 5, 3, 40),
             number(QStringLiteral("outer_radius"), QStringLiteral("Outer"), 1.0, 0.01, 20),
             number(QStringLiteral("inner_radius"), QStringLiteral("Inner"), 0.5, 0.01, 20)});

        add(QStringLiteral("Arc"), QStringLiteral("Arc"), QStringLiteral("Shapes"), ShapeKind::Arc,
            {number(QStringLiteral("radius"), QStringLiteral("Radius"), 1.0, 0.01, 20),
             number(QStringLiteral("start_angle"), QStringLiteral("From"), 0, -720, 720, 5),
             number(QStringLiteral("angle"), QStringLiteral("Sweep"), 90, -720, 720, 5)});

        add(QStringLiteral("Sector"), QStringLiteral("Sector"), QStringLiteral("Shapes"),
            ShapeKind::Sector,
            {number(QStringLiteral("radius"), QStringLiteral("Radius"), 1.0, 0.01, 20),
             number(QStringLiteral("start_angle"), QStringLiteral("From"), 0, -720, 720, 5),
             number(QStringLiteral("angle"), QStringLiteral("Sweep"), 90, -720, 720, 5)},
            commonParams(kManimWhite, 4.0, 1.0));

        add(QStringLiteral("Annulus"), QStringLiteral("Annulus"), QStringLiteral("Shapes"),
            ShapeKind::Annulus,
            {number(QStringLiteral("inner_radius"), QStringLiteral("Inner"), 0.6, 0.01, 20),
             number(QStringLiteral("outer_radius"), QStringLiteral("Outer"), 1.0, 0.01, 20)},
            commonParams(kManimWhite, 4.0, 1.0));

        add(QStringLiteral("Cross"), QStringLiteral("Cross"), QStringLiteral("Shapes"),
            ShapeKind::Cross,
            {number(QStringLiteral("size"), QStringLiteral("Size"), 1.0, 0.05, 20)});

        add(QStringLiteral("Elbow"), QStringLiteral("Elbow"), QStringLiteral("Shapes"),
            ShapeKind::Elbow,
            {number(QStringLiteral("width"), QStringLiteral("Size"), 0.5, 0.05, 10)});

        add(QStringLiteral("Angle"), QStringLiteral("Angle mark"), QStringLiteral("Shapes"),
            ShapeKind::Angle,
            {number(QStringLiteral("radius"), QStringLiteral("Radius"), 0.5, 0.05, 10),
             number(QStringLiteral("angle"), QStringLiteral("Sweep"), 90, -360, 360, 5)});

        add(QStringLiteral("Dot"), QStringLiteral("Dot"), QStringLiteral("Shapes"), ShapeKind::Dot,
            {number(QStringLiteral("radius"), QStringLiteral("Radius"), 0.08, 0.01, 5, 0.01)},
            commonParams(kManimWhite, 0.0, 1.0));

        // ------------------------------------------------------------ lines ---
        add(QStringLiteral("Line"), QStringLiteral("Line"), QStringLiteral("Lines"),
            ShapeKind::Line,
            {point(QStringLiteral("start"), QStringLiteral("Start"), QPointF(-1, 0)),
             point(QStringLiteral("end"), QStringLiteral("End"), QPointF(1, 0))});

        add(QStringLiteral("DashedLine"), QStringLiteral("Dashed line"), QStringLiteral("Lines"),
            ShapeKind::DashedLine,
            {point(QStringLiteral("start"), QStringLiteral("Start"), QPointF(-1, 0)),
             point(QStringLiteral("end"), QStringLiteral("End"), QPointF(1, 0)),
             number(QStringLiteral("dash_length"), QStringLiteral("Dash"), 0.15, 0.02, 2, 0.01)});

        add(QStringLiteral("Arrow"), QStringLiteral("Arrow"), QStringLiteral("Lines"),
            ShapeKind::Arrow,
            {point(QStringLiteral("start"), QStringLiteral("Start"), QPointF(-1, 0)),
             point(QStringLiteral("end"), QStringLiteral("End"), QPointF(1, 0)),
             number(QStringLiteral("tip_length"), QStringLiteral("Tip"), 0.25, 0.05, 3, 0.05)});

        add(QStringLiteral("DoubleArrow"), QStringLiteral("Double arrow"), QStringLiteral("Lines"),
            ShapeKind::DoubleArrow,
            {point(QStringLiteral("start"), QStringLiteral("Start"), QPointF(-1, 0)),
             point(QStringLiteral("end"), QStringLiteral("End"), QPointF(1, 0)),
             number(QStringLiteral("tip_length"), QStringLiteral("Tip"), 0.25, 0.05, 3, 0.05)});

        add(QStringLiteral("Vector"), QStringLiteral("Vector"), QStringLiteral("Lines"),
            ShapeKind::Vector,
            {point(QStringLiteral("direction"), QStringLiteral("To"), QPointF(1, 1)),
             number(QStringLiteral("tip_length"), QStringLiteral("Tip"), 0.25, 0.05, 3, 0.05)});

        add(QStringLiteral("CurvedArrow"), QStringLiteral("Curved arrow"), QStringLiteral("Lines"),
            ShapeKind::CurvedArrow,
            {point(QStringLiteral("start"), QStringLiteral("Start"), QPointF(-1, 0)),
             point(QStringLiteral("end"), QStringLiteral("End"), QPointF(1, 0)),
             number(QStringLiteral("angle"), QStringLiteral("Bend"), 45, -360, 360, 5)});

        // ------------------------------------------------------------- text ---
        add(QStringLiteral("Text"), QStringLiteral("Text"), QStringLiteral("Text"),
            ShapeKind::Text,
            {text(QStringLiteral("text"), QStringLiteral("Text"), QStringLiteral("Text")),
             number(QStringLiteral("font_size"), QStringLiteral("Size"), 48, 4, 400, 1),
             boolean(QStringLiteral("bold"), QStringLiteral("Bold"), false),
             boolean(QStringLiteral("italic"), QStringLiteral("Italic"), false)},
            commonParams(kManimWhite, 0.0, 1.0));

        add(QStringLiteral("MathTex"), QStringLiteral("Formula"), QStringLiteral("Text"),
            ShapeKind::MathText,
            {text(QStringLiteral("tex"), QStringLiteral("LaTeX"), QStringLiteral("x^2 + y^2 = r^2")),
             number(QStringLiteral("font_size"), QStringLiteral("Size"), 48, 4, 400, 1)},
            commonParams(kManimWhite, 0.0, 1.0));

        add(QStringLiteral("Tex"), QStringLiteral("LaTeX text"), QStringLiteral("Text"),
            ShapeKind::MathText,
            {text(QStringLiteral("tex"), QStringLiteral("LaTeX"), QStringLiteral("Hello")),
             number(QStringLiteral("font_size"), QStringLiteral("Size"), 48, 4, 400, 1)},
            commonParams(kManimWhite, 0.0, 1.0));

        add(QStringLiteral("Title"), QStringLiteral("Title"), QStringLiteral("Text"),
            ShapeKind::Text,
            {text(QStringLiteral("text"), QStringLiteral("Text"), QStringLiteral("Title")),
             number(QStringLiteral("font_size"), QStringLiteral("Size"), 60, 4, 400, 1)},
            commonParams(kManimWhite, 0.0, 1.0));

        add(QStringLiteral("Paragraph"), QStringLiteral("Paragraph"), QStringLiteral("Text"),
            ShapeKind::Paragraph,
            {text(QStringLiteral("text"), QStringLiteral("Lines"),
                  QStringLiteral("First line\nSecond line")),
             number(QStringLiteral("font_size"), QStringLiteral("Size"), 36, 4, 400, 1)},
            commonParams(kManimWhite, 0.0, 1.0));

        add(QStringLiteral("BulletedList"), QStringLiteral("Bulleted list"), QStringLiteral("Text"),
            ShapeKind::Paragraph,
            {text(QStringLiteral("text"), QStringLiteral("Items"),
                  QStringLiteral("One\nTwo\nThree")),
             number(QStringLiteral("font_size"), QStringLiteral("Size"), 36, 4, 400, 1)},
            commonParams(kManimWhite, 0.0, 1.0));

        add(QStringLiteral("Code"), QStringLiteral("Code block"), QStringLiteral("Text"),
            ShapeKind::CodeBlock,
            {text(QStringLiteral("code"), QStringLiteral("Code"),
                  QStringLiteral("def f(x):\n    return x * 2")),
             text(QStringLiteral("language"), QStringLiteral("Language"), QStringLiteral("python")),
             number(QStringLiteral("font_size"), QStringLiteral("Size"), 24, 4, 200, 1)},
            commonParams(kManimWhite, 0.0, 1.0));

        add(QStringLiteral("DecimalNumber"), QStringLiteral("Number"), QStringLiteral("Text"),
            ShapeKind::NumberText,
            {number(QStringLiteral("number"), QStringLiteral("Value"), 0, -1e6, 1e6, 0.1),
             integer(QStringLiteral("num_decimal_places"), QStringLiteral("Decimals"), 2, 0, 8),
             number(QStringLiteral("font_size"), QStringLiteral("Size"), 48, 4, 400, 1)},
            commonParams(kManimWhite, 0.0, 1.0));

        add(QStringLiteral("Integer"), QStringLiteral("Integer"), QStringLiteral("Text"),
            ShapeKind::NumberText,
            {number(QStringLiteral("number"), QStringLiteral("Value"), 0, -1e6, 1e6, 1),
             integer(QStringLiteral("num_decimal_places"), QStringLiteral("Decimals"), 0, 0, 0),
             number(QStringLiteral("font_size"), QStringLiteral("Size"), 48, 4, 400, 1)},
            commonParams(kManimWhite, 0.0, 1.0));

        // --------------------------------------------------------- diagrams ---
        add(QStringLiteral("NumberPlane"), QStringLiteral("Grid"), QStringLiteral("Graphs"),
            ShapeKind::NumberPlane,
            {number(QStringLiteral("x_range"), QStringLiteral("X range"), 7.0, 1, 30, 1),
             number(QStringLiteral("y_range"), QStringLiteral("Y range"), 4.0, 1, 30, 1)},
            commonParams(QColor(0x29, 0xAB, 0xCA), 2.0));

        add(QStringLiteral("Axes"), QStringLiteral("Axes"), QStringLiteral("Graphs"),
            ShapeKind::Axes,
            {number(QStringLiteral("x_range"), QStringLiteral("X range"), 6.0, 1, 30, 1),
             number(QStringLiteral("y_range"), QStringLiteral("Y range"), 3.0, 1, 30, 1),
             boolean(QStringLiteral("tips"), QStringLiteral("Arrow tips"), true)});

        add(QStringLiteral("NumberLine"), QStringLiteral("Number line"), QStringLiteral("Graphs"),
            ShapeKind::NumberLine,
            {number(QStringLiteral("length"), QStringLiteral("Length"), 8.0, 1, 30, 0.5),
             number(QStringLiteral("step"), QStringLiteral("Step"), 1.0, 0.1, 10, 0.1),
             boolean(QStringLiteral("include_numbers"), QStringLiteral("Numbers"), false)});

        add(QStringLiteral("BarChart"), QStringLiteral("Bar chart"), QStringLiteral("Graphs"),
            ShapeKind::BarChart,
            {text(QStringLiteral("values"), QStringLiteral("Values"), QStringLiteral("3, 1, 4, 1, 5")),
             number(QStringLiteral("y_range"), QStringLiteral("Max"), 6.0, 1, 100, 1)},
            commonParams(QColor(0x58, 0xC4, 0xDD), 2.0, 0.75));

        add(QStringLiteral("FunctionGraph"), QStringLiteral("Function"), QStringLiteral("Graphs"),
            ShapeKind::FunctionGraph,
            {text(QStringLiteral("function"), QStringLiteral("f(x)"), QStringLiteral("np.sin(x)")),
             number(QStringLiteral("x_min"), QStringLiteral("From"), -4.0, -30, 30, 0.5),
             number(QStringLiteral("x_max"), QStringLiteral("To"), 4.0, -30, 30, 0.5)},
            commonParams(QColor(0x83, 0xC1, 0x67), 4.0));

        add(QStringLiteral("Table"), QStringLiteral("Table"), QStringLiteral("Graphs"),
            ShapeKind::Table,
            {text(QStringLiteral("rows"), QStringLiteral("Rows"),
                  QStringLiteral("a, b\nc, d")),
             number(QStringLiteral("cell_width"), QStringLiteral("Cell width"), 1.2, 0.2, 10, 0.1),
             number(QStringLiteral("cell_height"), QStringLiteral("Cell height"), 0.8, 0.2, 10, 0.1)});

        add(QStringLiteral("Matrix"), QStringLiteral("Matrix"), QStringLiteral("Graphs"),
            ShapeKind::Matrix,
            {text(QStringLiteral("rows"), QStringLiteral("Rows"), QStringLiteral("1, 0\n0, 1")),
             number(QStringLiteral("cell_width"), QStringLiteral("Cell width"), 0.8, 0.2, 10, 0.1),
             number(QStringLiteral("cell_height"), QStringLiteral("Cell height"), 0.7, 0.2, 10, 0.1)});

        add(QStringLiteral("Brace"), QStringLiteral("Brace"), QStringLiteral("Annotation"),
            ShapeKind::Brace,
            {number(QStringLiteral("length"), QStringLiteral("Length"), 2.0, 0.2, 30, 0.1),
             number(QStringLiteral("depth"), QStringLiteral("Depth"), 0.3, 0.05, 3, 0.05)});

        add(QStringLiteral("Underline"), QStringLiteral("Underline"), QStringLiteral("Annotation"),
            ShapeKind::Underline,
            {number(QStringLiteral("length"), QStringLiteral("Length"), 2.0, 0.2, 30, 0.1)});

        add(QStringLiteral("SurroundingRectangle"), QStringLiteral("Highlight box"),
            QStringLiteral("Annotation"), ShapeKind::SurroundingBox,
            {number(QStringLiteral("width"), QStringLiteral("Width"), 2.0, 0.1, 30, 0.1),
             number(QStringLiteral("height"), QStringLiteral("Height"), 1.0, 0.1, 30, 0.1),
             number(QStringLiteral("buff"), QStringLiteral("Padding"), 0.1, 0.0, 3, 0.05)},
            commonParams(QColor(0xFF, 0xFF, 0x00), 3.0));

        // -------------------------------------------------------------- 3D ---
        add(QStringLiteral("Cube"), QStringLiteral("Cube"), QStringLiteral("3D"), ShapeKind::Cube,
            {number(QStringLiteral("side_length"), QStringLiteral("Side"), 2.0, 0.1, 20, 0.1)},
            commonParams(QColor(0x58, 0xC4, 0xDD), 3.0, 0.25));

        add(QStringLiteral("Sphere"), QStringLiteral("Sphere"), QStringLiteral("3D"),
            ShapeKind::Sphere,
            {number(QStringLiteral("radius"), QStringLiteral("Radius"), 1.0, 0.1, 20, 0.1)},
            commonParams(QColor(0x58, 0xC4, 0xDD), 2.0, 0.2));

        add(QStringLiteral("Cone"), QStringLiteral("Cone"), QStringLiteral("3D"), ShapeKind::Cone,
            {number(QStringLiteral("base_radius"), QStringLiteral("Radius"), 1.0, 0.1, 20, 0.1),
             number(QStringLiteral("height"), QStringLiteral("Height"), 2.0, 0.1, 20, 0.1)},
            commonParams(QColor(0x58, 0xC4, 0xDD), 2.0, 0.2));

        add(QStringLiteral("Cylinder"), QStringLiteral("Cylinder"), QStringLiteral("3D"),
            ShapeKind::Cylinder,
            {number(QStringLiteral("radius"), QStringLiteral("Radius"), 1.0, 0.1, 20, 0.1),
             number(QStringLiteral("height"), QStringLiteral("Height"), 2.0, 0.1, 20, 0.1)},
            commonParams(QColor(0x58, 0xC4, 0xDD), 2.0, 0.2));

        add(QStringLiteral("Torus"), QStringLiteral("Torus"), QStringLiteral("3D"), ShapeKind::Torus,
            {number(QStringLiteral("major_radius"), QStringLiteral("Major"), 1.0, 0.1, 20, 0.1),
             number(QStringLiteral("minor_radius"), QStringLiteral("Minor"), 0.35, 0.05, 10, 0.05)},
            commonParams(QColor(0x58, 0xC4, 0xDD), 2.0, 0.2));

        add(QStringLiteral("Prism"), QStringLiteral("Prism"), QStringLiteral("3D"), ShapeKind::Prism,
            {number(QStringLiteral("width"), QStringLiteral("Width"), 2.0, 0.1, 20, 0.1),
             number(QStringLiteral("height"), QStringLiteral("Height"), 1.0, 0.1, 20, 0.1),
             number(QStringLiteral("depth"), QStringLiteral("Depth"), 1.0, 0.1, 20, 0.1)},
            commonParams(QColor(0x58, 0xC4, 0xDD), 2.0, 0.2));

        add(QStringLiteral("Surface"), QStringLiteral("Surface"), QStringLiteral("3D"),
            ShapeKind::Surface3D,
            {text(QStringLiteral("function"), QStringLiteral("f(u, v)"),
                  QStringLiteral("[u, v, np.sin(u) * np.cos(v)]")),
             number(QStringLiteral("extent"), QStringLiteral("Extent"), 2.0, 0.2, 20, 0.1)},
            commonParams(QColor(0x58, 0xC4, 0xDD), 1.5, 0.2));

        add(QStringLiteral("ThreeDAxes"), QStringLiteral("3D axes"), QStringLiteral("3D"),
            ShapeKind::ThreeDAxes,
            {number(QStringLiteral("extent"), QStringLiteral("Extent"), 3.0, 0.5, 20, 0.5)});

        add(QStringLiteral("Line3D"), QStringLiteral("Line (3D)"), QStringLiteral("3D"),
            ShapeKind::Line3D,
            {point(QStringLiteral("start"), QStringLiteral("Start"), QPointF(-1, 0)),
             number(QStringLiteral("start_z"), QStringLiteral("Start z"), 0.0, -20, 20, 0.1),
             point(QStringLiteral("end"), QStringLiteral("End"), QPointF(1, 0)),
             number(QStringLiteral("end_z"), QStringLiteral("End z"), 1.0, -20, 20, 0.1)});

        add(QStringLiteral("Arrow3D"), QStringLiteral("Arrow (3D)"), QStringLiteral("3D"),
            ShapeKind::Arrow3D,
            {point(QStringLiteral("start"), QStringLiteral("Start"), QPointF(0, 0)),
             number(QStringLiteral("start_z"), QStringLiteral("Start z"), 0.0, -20, 20, 0.1),
             point(QStringLiteral("end"), QStringLiteral("End"), QPointF(1, 1)),
             number(QStringLiteral("end_z"), QStringLiteral("End z"), 1.0, -20, 20, 0.1)});

        add(QStringLiteral("Dot3D"), QStringLiteral("Dot (3D)"), QStringLiteral("3D"),
            ShapeKind::Dot3D,
            {number(QStringLiteral("radius"), QStringLiteral("Radius"), 0.1, 0.01, 5, 0.01)},
            commonParams(kManimWhite, 0.0, 1.0));

        for (const auto &solid : {std::pair{QStringLiteral("Tetrahedron"), 4},
                                  std::pair{QStringLiteral("Octahedron"), 8},
                                  std::pair{QStringLiteral("Icosahedron"), 20},
                                  std::pair{QStringLiteral("Dodecahedron"), 12}}) {
            add(solid.first, solid.first, QStringLiteral("3D"), ShapeKind::Polyhedron,
                {integer(QStringLiteral("faces"), QStringLiteral("Faces"), solid.second,
                         solid.second, solid.second),
                 number(QStringLiteral("edge_length"), QStringLiteral("Size"), 1.5, 0.1, 20, 0.1)},
                commonParams(QColor(0x58, 0xC4, 0xDD), 2.0, 0.2));
        }

        // ------------------------------------------------- groups and code ---
        add(QStringLiteral("VGroup"), QStringLiteral("Group"), QStringLiteral("Structure"),
            ShapeKind::Group, {});

        add(QStringLiteral("Custom"), QStringLiteral("Custom Python"), QStringLiteral("Structure"),
            ShapeKind::Custom,
            {text(QStringLiteral("expression"), QStringLiteral("Expression"),
                  QStringLiteral("Circle(radius=1)")),
             text(QStringLiteral("label"), QStringLiteral("Label"), QStringLiteral("Custom"))});

        // Manim typesets these through LaTeX. Flagged rather than left out:
        // they are perfectly good on a machine that has it.
        for (MobjectSpec &spec : result) {
            spec.requiresLatex = spec.shape == ShapeKind::MathText
                                 || spec.shape == ShapeKind::Matrix
                                 || spec.shape == ShapeKind::NumberText
                                 || spec.shape == ShapeKind::BarChart;
        }

        return result;
    }();
    return specs;
}

QStringList latexDependentIds()
{
    QStringList result;
    for (const MobjectSpec &spec : mobjects()) {
        if (spec.requiresLatex)
            result.append(spec.id);
    }
    return result;
}

const QVector<AnimationSpec> &animations()
{
    static const QVector<AnimationSpec> specs = [] {
        QVector<AnimationSpec> result;

        auto entrance = [&result](const QString &pythonName, const QString &display,
                                  Effect effect, const QVector<ParamSpec> &params = {}) {
            AnimationSpec spec =
                makeAnimation(pythonName, display, QStringLiteral("Entrance"), effect, params);
            spec.isEntrance = true;
            result.append(spec);
        };

        auto exit = [&result](const QString &pythonName, const QString &display, Effect effect,
                              const QVector<ParamSpec> &params = {}) {
            AnimationSpec spec =
                makeAnimation(pythonName, display, QStringLiteral("Exit"), effect, params);
            spec.isExit = true;
            result.append(spec);
        };

        auto motion = [&result](const QString &pythonName, const QString &display, Effect effect,
                                const QVector<ParamSpec> &params = {}) {
            result.append(makeAnimation(pythonName, display, QStringLiteral("Motion"), effect, params));
        };

        auto emphasis = [&result](const QString &pythonName, const QString &display,
                                  const QVector<ParamSpec> &params = {}) {
            result.append(
                makeAnimation(pythonName, display, QStringLiteral("Emphasis"), Effect::Emphasise, params));
        };

        // --------------------------------------------------------- entrance ---
        entrance(QStringLiteral("Create"), QStringLiteral("Create"), Effect::Draw);
        entrance(QStringLiteral("Write"), QStringLiteral("Write"), Effect::Draw);
        entrance(QStringLiteral("DrawBorderThenFill"), QStringLiteral("Draw border then fill"),
                 Effect::Draw);
        entrance(QStringLiteral("AddTextLetterByLetter"), QStringLiteral("Letter by letter"),
                 Effect::Draw);
        entrance(QStringLiteral("ShowIncreasingSubsets"), QStringLiteral("Increasing subsets"),
                 Effect::Draw);
        entrance(QStringLiteral("SpiralIn"), QStringLiteral("Spiral in"), Effect::FadeIn);
        entrance(QStringLiteral("FadeIn"), QStringLiteral("Fade in"), Effect::FadeIn,
                 {point(QStringLiteral("shift"), QStringLiteral("Shift from"), QPointF(0, 0))});
        entrance(QStringLiteral("GrowFromCenter"), QStringLiteral("Grow from centre"), Effect::Grow);
        entrance(QStringLiteral("GrowFromPoint"), QStringLiteral("Grow from point"), Effect::Grow,
                 {point(QStringLiteral("point"), QStringLiteral("From"), QPointF(0, 0))});
        entrance(QStringLiteral("GrowFromEdge"), QStringLiteral("Grow from edge"), Effect::Grow,
                 {point(QStringLiteral("edge"), QStringLiteral("Edge"), QPointF(0, -1))});
        entrance(QStringLiteral("GrowArrow"), QStringLiteral("Grow arrow"), Effect::Grow);
        entrance(QStringLiteral("SpinInFromNothing"), QStringLiteral("Spin in"), Effect::Grow);

        // ------------------------------------------------------------- exit ---
        exit(QStringLiteral("FadeOut"), QStringLiteral("Fade out"), Effect::FadeOut,
             {point(QStringLiteral("shift"), QStringLiteral("Shift to"), QPointF(0, 0))});
        exit(QStringLiteral("Uncreate"), QStringLiteral("Uncreate"), Effect::Draw);
        exit(QStringLiteral("Unwrite"), QStringLiteral("Unwrite"), Effect::Draw);
        exit(QStringLiteral("ShrinkToCenter"), QStringLiteral("Shrink to centre"), Effect::Grow);
        exit(QStringLiteral("RemoveTextLetterByLetter"), QStringLiteral("Remove letter by letter"),
             Effect::Draw);

        // ----------------------------------------------------------- motion ---
        motion(QStringLiteral("Shift"), QStringLiteral("Shift"), Effect::Shift,
               {point(QStringLiteral("by"), QStringLiteral("By"), QPointF(1, 0))});
        motion(QStringLiteral("MoveTo"), QStringLiteral("Move to"), Effect::MoveTo,
               {point(QStringLiteral("to"), QStringLiteral("To"), QPointF(0, 0))});
        motion(QStringLiteral("Rotate"), QStringLiteral("Rotate"), Effect::Rotate,
               {number(QStringLiteral("angle"), QStringLiteral("Angle"), 90, -1440, 1440, 5)});
        motion(QStringLiteral("ScaleBy"), QStringLiteral("Scale"), Effect::Scale,
               {number(QStringLiteral("factor"), QStringLiteral("Factor"), 1.5, 0.01, 20, 0.05)});
        motion(QStringLiteral("Recolor"), QStringLiteral("Change colour"), Effect::Recolor,
               {color(QStringLiteral("to"), QStringLiteral("To"), kManimYellow)});
        motion(QStringLiteral("SetOpacity"), QStringLiteral("Change opacity"), Effect::Fade,
               {number(QStringLiteral("to"), QStringLiteral("To"), 1.0, 0, 1, 0.05)});
        motion(QStringLiteral("MoveAlongPath"), QStringLiteral("Move along path"), Effect::Shift,
               {point(QStringLiteral("by"), QStringLiteral("By"), QPointF(2, 0))});

        // --------------------------------------------------------- emphasis ---
        emphasis(QStringLiteral("Indicate"), QStringLiteral("Indicate"),
                 {number(QStringLiteral("scale_factor"), QStringLiteral("Scale"), 1.2, 1, 4, 0.05)});
        emphasis(QStringLiteral("Flash"), QStringLiteral("Flash"));
        emphasis(QStringLiteral("Circumscribe"), QStringLiteral("Circumscribe"));
        emphasis(QStringLiteral("Wiggle"), QStringLiteral("Wiggle"));
        emphasis(QStringLiteral("ApplyWave"), QStringLiteral("Wave"));
        emphasis(QStringLiteral("FocusOn"), QStringLiteral("Focus on"));

        // ----------------------------------------------------------- timing ---
        result.append(makeAnimation(QStringLiteral("Wait"), QStringLiteral("Wait"),
                                    QStringLiteral("Timing"), Effect::Wait));

        // A block of Python run at this point in the sequence: loops,
        // conditions, anything the timeline has no shape for.
        AnimationSpec code = makeAnimation(
            QStringLiteral("Code"), QStringLiteral("Python block"), QStringLiteral("Logic"),
            Effect::Code,
            {text(QStringLiteral("body"), QStringLiteral("Python"),
                  QStringLiteral("for i in range(3):\n    self.play(Indicate(TARGET))"))});
        result.append(code);

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
