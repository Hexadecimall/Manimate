#include "ManimVocabulary.h"

namespace mn::ui {
namespace {

/// Build a set once, on first use, for the membership tests the highlighter
/// runs on every word of every repainted block.
QSet<QString> setOf(const QStringList &words)
{
    return {words.begin(), words.end()};
}

} // namespace

namespace manim {

const QStringList &mobjects()
{
    static const QStringList names = {
        // Shapes
        QStringLiteral("Circle"), QStringLiteral("Dot"), QStringLiteral("AnnularSector"),
        QStringLiteral("Annulus"), QStringLiteral("Arc"), QStringLiteral("ArcBetweenPoints"),
        QStringLiteral("ArcPolygon"), QStringLiteral("CurvedArrow"), QStringLiteral("CurvedDoubleArrow"),
        QStringLiteral("Ellipse"), QStringLiteral("Sector"), QStringLiteral("Square"),
        QStringLiteral("Rectangle"), QStringLiteral("RoundedRectangle"), QStringLiteral("Triangle"),
        QStringLiteral("Polygon"), QStringLiteral("Polygram"), QStringLiteral("RegularPolygon"),
        QStringLiteral("RegularPolygram"), QStringLiteral("Star"), QStringLiteral("Cutout"),
        QStringLiteral("Cross"), QStringLiteral("Elbow"), QStringLiteral("Angle"),
        QStringLiteral("RightAngle"), QStringLiteral("Line"), QStringLiteral("DashedLine"),
        QStringLiteral("TangentLine"), QStringLiteral("Arrow"), QStringLiteral("DoubleArrow"),
        QStringLiteral("Vector"), QStringLiteral("LabeledDot"), QStringLiteral("LabeledLine"),
        QStringLiteral("Underline"), QStringLiteral("SurroundingRectangle"), QStringLiteral("BackgroundRectangle"),
        // Text and formulae
        QStringLiteral("Text"), QStringLiteral("MarkupText"), QStringLiteral("Paragraph"),
        QStringLiteral("Tex"), QStringLiteral("MathTex"), QStringLiteral("SingleStringMathTex"),
        QStringLiteral("Title"), QStringLiteral("BulletedList"), QStringLiteral("Code"),
        QStringLiteral("DecimalNumber"), QStringLiteral("Integer"), QStringLiteral("Variable"),
        // Graphs and axes
        QStringLiteral("NumberLine"), QStringLiteral("UnitInterval"), QStringLiteral("Axes"),
        QStringLiteral("ThreeDAxes"), QStringLiteral("NumberPlane"), QStringLiteral("ComplexPlane"),
        QStringLiteral("PolarPlane"), QStringLiteral("BarChart"), QStringLiteral("ImplicitFunction"),
        QStringLiteral("ParametricFunction"), QStringLiteral("FunctionGraph"),
        QStringLiteral("Graph"), QStringLiteral("DiGraph"),
        // Tables and matrices
        QStringLiteral("Table"), QStringLiteral("MobjectTable"), QStringLiteral("IntegerTable"),
        QStringLiteral("DecimalTable"), QStringLiteral("MathTable"), QStringLiteral("Matrix"),
        QStringLiteral("DecimalMatrix"), QStringLiteral("IntegerMatrix"), QStringLiteral("MobjectMatrix"),
        // Braces and annotation
        QStringLiteral("Brace"), QStringLiteral("BraceBetweenPoints"), QStringLiteral("BraceLabel"),
        QStringLiteral("BraceText"), QStringLiteral("ArrowVectorField"), QStringLiteral("VectorField"),
        QStringLiteral("StreamLines"),
        // Three dimensions
        QStringLiteral("Cube"), QStringLiteral("Sphere"), QStringLiteral("Cone"),
        QStringLiteral("Cylinder"), QStringLiteral("Line3D"), QStringLiteral("Arrow3D"),
        QStringLiteral("Dot3D"), QStringLiteral("Torus"), QStringLiteral("Surface"),
        QStringLiteral("Prism"), QStringLiteral("Polyhedron"), QStringLiteral("Tetrahedron"),
        QStringLiteral("Octahedron"), QStringLiteral("Icosahedron"), QStringLiteral("Dodecahedron"),
        QStringLiteral("ThreeDVMobject"),
        // Containers and bases
        QStringLiteral("Mobject"), QStringLiteral("VMobject"), QStringLiteral("VGroup"),
        QStringLiteral("Group"), QStringLiteral("VDict"), QStringLiteral("ImageMobject"),
        QStringLiteral("SVGMobject"), QStringLiteral("ValueTracker"), QStringLiteral("ComplexValueTracker"),
        QStringLiteral("Point"), QStringLiteral("PointCloudDot"),
    };
    return names;
}

const QStringList &animations()
{
    static const QStringList names = {
        QStringLiteral("Animation"), QStringLiteral("Create"), QStringLiteral("Uncreate"),
        QStringLiteral("DrawBorderThenFill"), QStringLiteral("Write"), QStringLiteral("Unwrite"),
        QStringLiteral("AddTextLetterByLetter"), QStringLiteral("RemoveTextLetterByLetter"),
        QStringLiteral("AddTextWordByWord"), QStringLiteral("ShowIncreasingSubsets"),
        QStringLiteral("ShowSubmobjectsOneByOne"), QStringLiteral("SpiralIn"),
        QStringLiteral("FadeIn"), QStringLiteral("FadeOut"),
        QStringLiteral("Transform"), QStringLiteral("ReplacementTransform"),
        QStringLiteral("TransformFromCopy"), QStringLiteral("ClockwiseTransform"),
        QStringLiteral("CounterclockwiseTransform"), QStringLiteral("MoveToTarget"),
        QStringLiteral("ApplyMethod"), QStringLiteral("ApplyPointwiseFunction"),
        QStringLiteral("ApplyMatrix"), QStringLiteral("ApplyComplexFunction"),
        QStringLiteral("ApplyFunction"), QStringLiteral("CyclicReplace"), QStringLiteral("Swap"),
        QStringLiteral("TransformMatchingShapes"), QStringLiteral("TransformMatchingTex"),
        QStringLiteral("FadeTransform"), QStringLiteral("FadeTransformPieces"),
        QStringLiteral("GrowFromCenter"), QStringLiteral("GrowFromPoint"), QStringLiteral("GrowFromEdge"),
        QStringLiteral("GrowArrow"), QStringLiteral("SpinInFromNothing"), QStringLiteral("ShrinkToCenter"),
        QStringLiteral("Rotate"), QStringLiteral("Rotating"),
        QStringLiteral("Indicate"), QStringLiteral("Flash"), QStringLiteral("ShowPassingFlash"),
        QStringLiteral("Circumscribe"), QStringLiteral("ApplyWave"), QStringLiteral("Wiggle"),
        QStringLiteral("FocusOn"), QStringLiteral("Blink"), QStringLiteral("Broadcast"),
        QStringLiteral("MoveAlongPath"), QStringLiteral("Homotopy"), QStringLiteral("ComplexHomotopy"),
        QStringLiteral("SmoothedVectorizedHomotopy"), QStringLiteral("PhaseFlow"),
        QStringLiteral("Restore"), QStringLiteral("AnimationGroup"), QStringLiteral("LaggedStart"),
        QStringLiteral("LaggedStartMap"), QStringLiteral("Succession"), QStringLiteral("Wait"),
        QStringLiteral("UpdateFromFunc"), QStringLiteral("UpdateFromAlphaFunc"),
        QStringLiteral("MaintainPositionRelativeTo"), QStringLiteral("ChangingDecimal"),
        QStringLiteral("ChangeDecimalToValue"), QStringLiteral("Count"), QStringLiteral("Unform"),
    };
    return names;
}

const QStringList &scenes()
{
    static const QStringList names = {
        QStringLiteral("Scene"),           QStringLiteral("ThreeDScene"),
        QStringLiteral("MovingCameraScene"), QStringLiteral("ZoomedScene"),
        QStringLiteral("VectorScene"),     QStringLiteral("LinearTransformationScene"),
        QStringLiteral("SpecialThreeDScene"),
    };
    return names;
}

const QStringList &colors()
{
    static const QStringList names = [] {
        QStringList result = {
            QStringLiteral("WHITE"),      QStringLiteral("BLACK"),      QStringLiteral("GRAY"),
            QStringLiteral("GREY"),       QStringLiteral("DARK_GRAY"),  QStringLiteral("DARK_GREY"),
            QStringLiteral("DARKER_GRAY"), QStringLiteral("DARKER_GREY"), QStringLiteral("LIGHT_GRAY"),
            QStringLiteral("LIGHT_GREY"), QStringLiteral("LIGHTER_GRAY"), QStringLiteral("LIGHTER_GREY"),
            QStringLiteral("PINK"),       QStringLiteral("LIGHT_PINK"), QStringLiteral("ORANGE"),
            QStringLiteral("GRAY_BROWN"), QStringLiteral("GREY_BROWN"), QStringLiteral("DARK_BROWN"),
            QStringLiteral("LIGHT_BROWN"), QStringLiteral("PURE_RED"),  QStringLiteral("PURE_GREEN"),
            QStringLiteral("PURE_BLUE"),  QStringLiteral("YELLOW"),     QStringLiteral("GOLD"),
            QStringLiteral("RED"),        QStringLiteral("MAROON"),     QStringLiteral("PURPLE"),
            QStringLiteral("BLUE"),       QStringLiteral("TEAL"),       QStringLiteral("GREEN"),
        };
        // Manim gives most hues five shades, from A (lightest) to E (darkest).
        for (const QString &family : {QStringLiteral("BLUE"), QStringLiteral("TEAL"),
                                      QStringLiteral("GREEN"), QStringLiteral("YELLOW"),
                                      QStringLiteral("GOLD"), QStringLiteral("RED"),
                                      QStringLiteral("MAROON"), QStringLiteral("PURPLE"),
                                      QStringLiteral("GREY"), QStringLiteral("GRAY")}) {
            for (const QChar shade : {u'A', u'B', u'C', u'D', u'E'})
                result.append(family + QLatin1Char('_') + shade);
        }
        return result;
    }();
    return names;
}

const QStringList &constants()
{
    static const QStringList names = {
        QStringLiteral("UP"),    QStringLiteral("DOWN"),  QStringLiteral("LEFT"),
        QStringLiteral("RIGHT"), QStringLiteral("IN"),    QStringLiteral("OUT"),
        QStringLiteral("ORIGIN"), QStringLiteral("UL"),   QStringLiteral("UR"),
        QStringLiteral("DL"),    QStringLiteral("DR"),
        QStringLiteral("X_AXIS"), QStringLiteral("Y_AXIS"), QStringLiteral("Z_AXIS"),
        QStringLiteral("PI"),    QStringLiteral("TAU"),   QStringLiteral("DEGREES"),
        QStringLiteral("SMALL_BUFF"), QStringLiteral("MED_SMALL_BUFF"),
        QStringLiteral("MED_LARGE_BUFF"), QStringLiteral("LARGE_BUFF"),
        QStringLiteral("DEFAULT_MOBJECT_TO_EDGE_BUFFER"),
        QStringLiteral("DEFAULT_MOBJECT_TO_MOBJECT_BUFFER"),
        QStringLiteral("DEFAULT_DOT_RADIUS"), QStringLiteral("DEFAULT_STROKE_WIDTH"),
        QStringLiteral("DEFAULT_FONT_SIZE"), QStringLiteral("config"),
    };
    return names;
}

const QStringList &rateFunctions()
{
    static const QStringList names = {
        QStringLiteral("linear"), QStringLiteral("smooth"), QStringLiteral("smoothstep"),
        QStringLiteral("smoothererstep"), QStringLiteral("rush_into"), QStringLiteral("rush_from"),
        QStringLiteral("slow_into"), QStringLiteral("double_smooth"), QStringLiteral("there_and_back"),
        QStringLiteral("there_and_back_with_pause"), QStringLiteral("running_start"),
        QStringLiteral("wiggle"), QStringLiteral("exponential_decay"), QStringLiteral("lingering"),
        QStringLiteral("ease_in_sine"), QStringLiteral("ease_out_sine"), QStringLiteral("ease_in_out_sine"),
        QStringLiteral("ease_in_quad"), QStringLiteral("ease_out_quad"), QStringLiteral("ease_in_out_quad"),
        QStringLiteral("ease_in_cubic"), QStringLiteral("ease_out_cubic"), QStringLiteral("ease_in_out_cubic"),
        QStringLiteral("ease_in_quart"), QStringLiteral("ease_out_quart"), QStringLiteral("ease_in_out_quart"),
        QStringLiteral("ease_in_expo"), QStringLiteral("ease_out_expo"), QStringLiteral("ease_in_out_expo"),
        QStringLiteral("ease_in_circ"), QStringLiteral("ease_out_circ"), QStringLiteral("ease_in_out_circ"),
        QStringLiteral("ease_in_back"), QStringLiteral("ease_out_back"), QStringLiteral("ease_in_out_back"),
        QStringLiteral("ease_in_elastic"), QStringLiteral("ease_out_elastic"), QStringLiteral("ease_in_out_elastic"),
        QStringLiteral("ease_in_bounce"), QStringLiteral("ease_out_bounce"), QStringLiteral("ease_in_out_bounce"),
    };
    return names;
}

const QStringList &sceneMethods()
{
    static const QStringList names = {
        QStringLiteral("construct"), QStringLiteral("play"), QStringLiteral("wait"),
        QStringLiteral("wait_until"), QStringLiteral("add"), QStringLiteral("remove"),
        QStringLiteral("clear"), QStringLiteral("bring_to_front"), QStringLiteral("bring_to_back"),
        QStringLiteral("add_foreground_mobject"), QStringLiteral("add_fixed_in_frame_mobjects"),
        QStringLiteral("add_fixed_orientation_mobjects"), QStringLiteral("set_camera_orientation"),
        QStringLiteral("move_camera"), QStringLiteral("begin_ambient_camera_rotation"),
        QStringLiteral("stop_ambient_camera_rotation"), QStringLiteral("next_section"),
    };
    return names;
}

const QStringList &mobjectMethods()
{
    static const QStringList names = {
        QStringLiteral("shift"), QStringLiteral("move_to"), QStringLiteral("next_to"),
        QStringLiteral("to_edge"), QStringLiteral("to_corner"), QStringLiteral("align_to"),
        QStringLiteral("scale"), QStringLiteral("rotate"), QStringLiteral("flip"),
        QStringLiteral("stretch"), QStringLiteral("set_color"), QStringLiteral("set_fill"),
        QStringLiteral("set_stroke"), QStringLiteral("set_opacity"), QStringLiteral("set_z_index"),
        QStringLiteral("get_center"), QStringLiteral("get_top"), QStringLiteral("get_bottom"),
        QStringLiteral("get_left"), QStringLiteral("get_right"), QStringLiteral("get_corner"),
        QStringLiteral("copy"), QStringLiteral("become"), QStringLiteral("animate"),
        QStringLiteral("add_updater"), QStringLiteral("remove_updater"), QStringLiteral("clear_updaters"),
        QStringLiteral("save_state"), QStringLiteral("restore"), QStringLiteral("arrange"),
        QStringLiteral("arrange_in_grid"), QStringLiteral("shuffle"), QStringLiteral("fade"),
        QStringLiteral("get_graph"), QStringLiteral("plot"), QStringLiteral("get_axis_labels"),
    };
    return names;
}

const QStringList &all()
{
    static const QStringList names = [] {
        QStringList result;
        for (const QStringList *group : {&mobjects(), &animations(), &scenes(), &colors(),
                                         &constants(), &rateFunctions(), &sceneMethods(),
                                         &mobjectMethods()}) {
            result += *group;
        }
        result.removeDuplicates();
        return result;
    }();
    return names;
}

bool isMobject(const QString &word)
{
    static const QSet<QString> lookup = setOf(mobjects());
    return lookup.contains(word);
}

bool isAnimation(const QString &word)
{
    static const QSet<QString> lookup = setOf(animations());
    return lookup.contains(word);
}

bool isScene(const QString &word)
{
    static const QSet<QString> lookup = setOf(scenes());
    return lookup.contains(word);
}

bool isColor(const QString &word)
{
    static const QSet<QString> lookup = setOf(colors());
    return lookup.contains(word);
}

bool isConstant(const QString &word)
{
    static const QSet<QString> lookup = setOf(constants());
    return lookup.contains(word);
}

bool isRateFunction(const QString &word)
{
    static const QSet<QString> lookup = setOf(rateFunctions());
    return lookup.contains(word);
}

} // namespace manim

namespace python {

const QStringList &keywords()
{
    static const QStringList names = {
        QStringLiteral("False"),  QStringLiteral("None"),   QStringLiteral("True"),
        QStringLiteral("and"),    QStringLiteral("as"),     QStringLiteral("assert"),
        QStringLiteral("async"),  QStringLiteral("await"),  QStringLiteral("break"),
        QStringLiteral("class"),  QStringLiteral("continue"), QStringLiteral("def"),
        QStringLiteral("del"),    QStringLiteral("elif"),   QStringLiteral("else"),
        QStringLiteral("except"), QStringLiteral("finally"), QStringLiteral("for"),
        QStringLiteral("from"),   QStringLiteral("global"), QStringLiteral("if"),
        QStringLiteral("import"), QStringLiteral("in"),     QStringLiteral("is"),
        QStringLiteral("lambda"), QStringLiteral("nonlocal"), QStringLiteral("not"),
        QStringLiteral("or"),     QStringLiteral("pass"),   QStringLiteral("raise"),
        QStringLiteral("return"), QStringLiteral("try"),    QStringLiteral("while"),
        QStringLiteral("with"),   QStringLiteral("yield"),
    };
    return names;
}

const QStringList &softKeywords()
{
    static const QStringList names = {
        QStringLiteral("match"), QStringLiteral("case"), QStringLiteral("type"), QStringLiteral("_"),
    };
    return names;
}

const QStringList &builtins()
{
    static const QStringList names = {
        QStringLiteral("abs"), QStringLiteral("all"), QStringLiteral("any"), QStringLiteral("bool"),
        QStringLiteral("bytes"), QStringLiteral("callable"), QStringLiteral("chr"),
        QStringLiteral("classmethod"), QStringLiteral("dict"), QStringLiteral("dir"),
        QStringLiteral("divmod"), QStringLiteral("enumerate"), QStringLiteral("filter"),
        QStringLiteral("float"), QStringLiteral("format"), QStringLiteral("frozenset"),
        QStringLiteral("getattr"), QStringLiteral("hasattr"), QStringLiteral("hash"),
        QStringLiteral("help"), QStringLiteral("hex"), QStringLiteral("id"), QStringLiteral("input"),
        QStringLiteral("int"), QStringLiteral("isinstance"), QStringLiteral("issubclass"),
        QStringLiteral("iter"), QStringLiteral("len"), QStringLiteral("list"), QStringLiteral("map"),
        QStringLiteral("max"), QStringLiteral("min"), QStringLiteral("next"), QStringLiteral("object"),
        QStringLiteral("open"), QStringLiteral("ord"), QStringLiteral("pow"), QStringLiteral("print"),
        QStringLiteral("property"), QStringLiteral("range"), QStringLiteral("repr"),
        QStringLiteral("reversed"), QStringLiteral("round"), QStringLiteral("set"),
        QStringLiteral("setattr"), QStringLiteral("slice"), QStringLiteral("sorted"),
        QStringLiteral("staticmethod"), QStringLiteral("str"), QStringLiteral("sum"),
        QStringLiteral("super"), QStringLiteral("tuple"), QStringLiteral("type"), QStringLiteral("zip"),
        QStringLiteral("self"), QStringLiteral("cls"),
    };
    return names;
}

bool isKeyword(const QString &word)
{
    static const QSet<QString> lookup = setOf(keywords());
    return lookup.contains(word);
}

bool isBuiltin(const QString &word)
{
    static const QSet<QString> lookup = setOf(builtins());
    return lookup.contains(word);
}

} // namespace python
} // namespace mn::ui
