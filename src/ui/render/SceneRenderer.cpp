#include "SceneRenderer.h"

#include "Catalog.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QtMath>

namespace mn::ui {
namespace {

using catalog::ShapeKind;

double numberParam(const QVariantMap &params, const QString &id, double fallback = 0.0)
{
    const QVariant value = params.value(id);
    return value.isValid() ? value.toDouble() : fallback;
}

QPointF pointParam(const QVariantMap &params, const QString &id)
{
    return params.value(id).toPointF();
}

QColor colorParam(const QVariantMap &params, const QString &id, const QColor &fallback)
{
    const QColor value = params.value(id).value<QColor>();
    return value.isValid() ? value : fallback;
}

/// A regular polygon with its first vertex pointing up, as Manim draws them.
QPainterPath regularPolygon(int sides, double radius)
{
    QPainterPath path;
    sides = qMax(3, sides);
    for (int i = 0; i < sides; ++i) {
        const double angle = M_PI / 2.0 + (2.0 * M_PI * i) / sides;
        const QPointF vertex(radius * std::cos(angle), radius * std::sin(angle));
        if (i == 0)
            path.moveTo(vertex);
        else
            path.lineTo(vertex);
    }
    path.closeSubpath();
    return path;
}

QPainterPath star(int points, double outer, double inner)
{
    QPainterPath path;
    points = qMax(3, points);
    for (int i = 0; i < points * 2; ++i) {
        const double radius = (i % 2 == 0) ? outer : inner;
        const double angle = M_PI / 2.0 + (M_PI * i) / points;
        const QPointF vertex(radius * std::cos(angle), radius * std::sin(angle));
        if (i == 0)
            path.moveTo(vertex);
        else
            path.lineTo(vertex);
    }
    path.closeSubpath();
    return path;
}

QPainterPath arrow(const QPointF &start, const QPointF &end, double tipLength)
{
    QPainterPath path;
    path.moveTo(start);
    path.lineTo(end);

    const QLineF shaft(start, end);
    if (shaft.length() <= 0.0)
        return path;

    const double angle = std::atan2(end.y() - start.y(), end.x() - start.x());
    constexpr double spread = M_PI / 7.0;
    for (const double side : {angle + M_PI - spread, angle + M_PI + spread}) {
        path.moveTo(end);
        path.lineTo(end + QPointF(tipLength * std::cos(side), tipLength * std::sin(side)));
    }
    return path;
}

/// The first `fraction` of a path, for animations that draw an outline into
/// existence. Sampled rather than cut exactly, which is invisible at any size
/// the canvas actually shows.
QPainterPath partialPath(const QPainterPath &path, double fraction)
{
    if (fraction >= 1.0)
        return path;
    if (fraction <= 0.0)
        return {};

    constexpr int kSamples = 160;
    const int steps = qMax(2, int(kSamples * fraction));

    QPainterPath result;
    result.moveTo(path.pointAtPercent(0.0));
    for (int i = 1; i <= steps; ++i) {
        const double t = fraction * (double(i) / steps);
        result.lineTo(path.pointAtPercent(t));
    }
    return result;
}

QPainterPath textPath(const QString &text, double fontSize, bool bold, bool italic)
{
    QFont font;
    // Lay the glyphs out at a comfortable pixel size, then scale the result
    // into scene units, so the path stays smooth at any zoom.
    constexpr double kLayoutPixels = 100.0;
    font.setPixelSize(int(kLayoutPixels));
    font.setBold(bold);
    font.setItalic(italic);

    QPainterPath path;
    const QStringList lines = text.split(QLatin1Char('\n'));
    const QFontMetricsF metrics(font);
    double y = 0.0;
    for (const QString &line : lines) {
        QPainterPath linePath;
        linePath.addText(0, 0, font, line);
        // Centre each line horizontally.
        const QRectF bounds = linePath.boundingRect();
        path.addPath(linePath.translated(-bounds.center().x(), y));
        y += metrics.height();
    }

    const QRectF bounds = path.boundingRect();
    if (bounds.isEmpty())
        return {};

    const double unitsPerPixel = (fontSize * SceneRenderer::kFontUnitsPerPoint) / kLayoutPixels;

    QTransform transform;
    transform.scale(unitsPerPixel, -unitsPerPixel);   // y up
    QPainterPath scaled = transform.map(path);

    // Centre the whole block on the object's position.
    const QRectF scaledBounds = scaled.boundingRect();
    return scaled.translated(-scaledBounds.center());
}

QPainterPath gridPath(double xRange, double yRange, bool axesOnly, bool tips)
{
    QPainterPath path;

    if (!axesOnly) {
        for (int x = int(-xRange); x <= int(xRange); ++x) {
            path.moveTo(x, -yRange);
            path.lineTo(x, yRange);
        }
        for (int y = int(-yRange); y <= int(yRange); ++y) {
            path.moveTo(-xRange, y);
            path.lineTo(xRange, y);
        }
        return path;
    }

    if (tips) {
        path.addPath(arrow(QPointF(-xRange, 0), QPointF(xRange, 0), 0.2));
        path.addPath(arrow(QPointF(0, -yRange), QPointF(0, yRange), 0.2));
    } else {
        path.moveTo(-xRange, 0);
        path.lineTo(xRange, 0);
        path.moveTo(0, -yRange);
        path.lineTo(0, yRange);
    }

    // Tick marks, so axes read as axes rather than a cross.
    for (int x = int(-xRange) + 1; x < int(xRange); ++x) {
        if (x == 0)
            continue;
        path.moveTo(x, -0.1);
        path.lineTo(x, 0.1);
    }
    for (int y = int(-yRange) + 1; y < int(yRange); ++y) {
        if (y == 0)
            continue;
        path.moveTo(-0.1, y);
        path.lineTo(0.1, y);
    }
    return path;
}

} // namespace

QRectF SceneRenderer::frameRectFor(const Document &document, const QRectF &viewport)
{
    const double aspect = document.render.height > 0
                              ? double(document.render.width) / double(document.render.height)
                              : 16.0 / 9.0;

    double width = viewport.width();
    double height = width / aspect;
    if (height > viewport.height()) {
        height = viewport.height();
        width = height * aspect;
    }

    return QRectF(viewport.center().x() - width / 2.0, viewport.center().y() - height / 2.0,
                  width, height);
}

QTransform SceneRenderer::sceneToPixels(const Document &document, const QRectF &frameRect)
{
    const double unitsTall = RenderSettings::kFrameHeightUnits;
    const double pixelsPerUnit = frameRect.height() / unitsTall;

    QTransform transform;
    transform.translate(frameRect.center().x(), frameRect.center().y());
    // Manim's y axis points up; the screen's points down.
    transform.scale(pixelsPerUnit, -pixelsPerUnit);
    return transform;
}

QPainterPath SceneRenderer::shapeOf(const evaluator::ObjectState &state)
{
    const catalog::MobjectSpec *spec = catalog::findMobject(state.type);
    if (!spec)
        return {};

    const QVariantMap &p = state.params;
    QPainterPath path;

    switch (spec->shape) {
    case ShapeKind::Circle: {
        const double radius = numberParam(p, QStringLiteral("radius"), 1.0);
        path.addEllipse(QPointF(0, 0), radius, radius);
        break;
    }
    case ShapeKind::Ellipse:
        path.addEllipse(QPointF(0, 0), numberParam(p, QStringLiteral("width"), 2.0) / 2.0,
                        numberParam(p, QStringLiteral("height"), 1.0) / 2.0);
        break;
    case ShapeKind::Rectangle: {
        const double width = p.contains(QStringLiteral("side_length"))
                                 ? numberParam(p, QStringLiteral("side_length"), 2.0)
                                 : numberParam(p, QStringLiteral("width"), 4.0);
        const double height = p.contains(QStringLiteral("side_length"))
                                  ? width
                                  : numberParam(p, QStringLiteral("height"), 2.0);
        path.addRect(-width / 2.0, -height / 2.0, width, height);
        break;
    }
    case ShapeKind::RoundedRectangle: {
        const double width = numberParam(p, QStringLiteral("width"), 4.0);
        const double height = numberParam(p, QStringLiteral("height"), 2.0);
        const double radius = numberParam(p, QStringLiteral("corner_radius"), 0.5);
        path.addRoundedRect(QRectF(-width / 2.0, -height / 2.0, width, height), radius, radius);
        break;
    }
    case ShapeKind::RegularPolygon:
        path = regularPolygon(p.value(QStringLiteral("n")).toInt(),
                              numberParam(p, QStringLiteral("radius"), 1.0));
        break;
    case ShapeKind::Star:
        path = star(p.value(QStringLiteral("n")).toInt(),
                    numberParam(p, QStringLiteral("outer_radius"), 1.0),
                    numberParam(p, QStringLiteral("inner_radius"), 0.5));
        break;
    case ShapeKind::Line:
        path.moveTo(pointParam(p, QStringLiteral("start")));
        path.lineTo(pointParam(p, QStringLiteral("end")));
        break;
    case ShapeKind::Arrow:
        path = arrow(pointParam(p, QStringLiteral("start")), pointParam(p, QStringLiteral("end")),
                     numberParam(p, QStringLiteral("tip_length"), 0.25));
        break;
    case ShapeKind::Dot: {
        const double radius = numberParam(p, QStringLiteral("radius"), 0.08);
        path.addEllipse(QPointF(0, 0), radius, radius);
        break;
    }
    case ShapeKind::Text:
        path = textPath(p.value(QStringLiteral("text")).toString(),
                        numberParam(p, QStringLiteral("font_size"), 48),
                        p.value(QStringLiteral("bold")).toBool(),
                        p.value(QStringLiteral("italic")).toBool());
        break;
    case ShapeKind::MathText:
        // LaTeX is not typeset here; the shape stands in for it until the real
        // render, and reads as a formula rather than pretending to be one.
        path = textPath(p.value(QStringLiteral("tex")).toString(),
                        numberParam(p, QStringLiteral("font_size"), 48), false, true);
        break;
    case ShapeKind::NumberPlane:
        path = gridPath(numberParam(p, QStringLiteral("x_range"), 7.0),
                        numberParam(p, QStringLiteral("y_range"), 4.0), false, false);
        break;
    case ShapeKind::Axes:
        path = gridPath(numberParam(p, QStringLiteral("x_range"), 6.0),
                        numberParam(p, QStringLiteral("y_range"), 3.0), true,
                        p.value(QStringLiteral("tips")).toBool());
        break;
    }

    return path;
}

bool SceneRenderer::positionIsAnOffset(const QString &type)
{
    const catalog::MobjectSpec *spec = catalog::findMobject(type);
    if (!spec)
        return false;
    // A line and an arrow already say where they are, through their endpoints.
    return spec->shape == ShapeKind::Line || spec->shape == ShapeKind::Arrow;
}

QTransform SceneRenderer::transformOf(const evaluator::ObjectState &state,
                                      const QPainterPath &shape)
{
    const QPointF position = state.params.value(QStringLiteral("position")).toPointF() + state.offset;
    const double rotation = numberParam(state.params, QStringLiteral("rotation"), 0.0)
                            + state.rotationDegrees;
    const double scale = numberParam(state.params, QStringLiteral("scale"), 1.0) * state.scale;

    QTransform transform;
    transform.translate(position.x(), position.y());
    transform.rotate(rotation);
    transform.scale(scale, scale);

    // Bring the bounding box's centre to the origin first, so the position
    // names the object's centre and rotation turns about it, as in Manim.
    if (!positionIsAnOffset(state.type) && !shape.isEmpty()) {
        const QPointF centre = shape.boundingRect().center();
        transform.translate(-centre.x(), -centre.y());
    }
    return transform;
}

void SceneRenderer::renderObject(QPainter &painter, const evaluator::ObjectState &state,
                                 const QTransform &toPixels)
{
    QPainterPath path = shapeOf(state);
    if (path.isEmpty())
        return;

    if (state.drawProgress < 1.0)
        path = partialPath(path, state.drawProgress);

    const QTransform full = transformOf(state, shapeOf(state)) * toPixels;
    const QPainterPath pixels = full.map(path);

    const catalog::MobjectSpec *spec = catalog::findMobject(state.type);
    const bool isText = spec
                        && (spec->shape == ShapeKind::Text || spec->shape == ShapeKind::MathText);

    QColor stroke = state.colorOverride.isValid()
                        ? state.colorOverride
                        : colorParam(state.params, QStringLiteral("color"), Qt::white);
    // Manim fills with the mobject's own colour, so Circle(color=BLUE,
    // fill_opacity=0.5) is a blue disc rather than a grey one.
    const QColor fill = stroke;

    const double fillOpacity = numberParam(state.params, QStringLiteral("fill_opacity"), 0.0);
    const double strokeWidth = numberParam(state.params, QStringLiteral("stroke_width"), 4.0);

    painter.save();
    painter.setOpacity(state.opacity);

    if (fillOpacity > 0.0) {
        QColor brush = fill;
        brush.setAlphaF(brush.alphaF() * fillOpacity);
        painter.setBrush(brush);
        painter.setPen(Qt::NoPen);
        painter.drawPath(pixels);
    }

    // Text is a filled outline; everything else is a stroke.
    if (isText && fillOpacity <= 0.0) {
        painter.setBrush(stroke);
        painter.setPen(Qt::NoPen);
        painter.drawPath(pixels);
    } else if (strokeWidth > 0.0) {
        // Manim measures stroke in its own units; this factor lines the two up
        // closely enough to compose against.
        const double widthInPixels = strokeWidth * 0.01 * toPixels.m11();
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(stroke, qMax(0.75, widthInPixels), Qt::SolidLine, Qt::RoundCap,
                            Qt::RoundJoin));
        painter.drawPath(pixels);
    }

    painter.restore();
}

void SceneRenderer::render(QPainter &painter, const Document &document, double time,
                           const QRectF &frameRect)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setClipRect(frameRect);
    painter.fillRect(frameRect, document.render.background);

    const QTransform toPixels = sceneToPixels(document, frameRect);
    for (const evaluator::ObjectState &state : evaluator::evaluate(document, time))
        renderObject(painter, state, toPixels);

    painter.restore();
}

QRectF SceneRenderer::boundsInPixels(const evaluator::ObjectState &state, const QTransform &toPixels)
{
    const QPainterPath path = shapeOf(state);
    if (path.isEmpty())
        return {};
    return (transformOf(state, path) * toPixels).map(path).boundingRect();
}

ObjectId SceneRenderer::objectAt(const QVector<evaluator::ObjectState> &states,
                                 const QPointF &scenePoint)
{
    // Topmost first, so clicking picks what the eye picks.
    for (int i = states.size() - 1; i >= 0; --i) {
        const evaluator::ObjectState &state = states.at(i);
        QPainterPath path = shapeOf(state);
        if (path.isEmpty())
            continue;

        const QPainterPath placed = transformOf(state, path).map(path);
        if (placed.contains(scenePoint))
            return state.id;

        // Unfilled outlines need a tolerance, or thin shapes are unclickable.
        QPainterPathStroker stroker;
        stroker.setWidth(0.18);
        if (stroker.createStroke(placed).contains(scenePoint))
            return state.id;
    }
    return kInvalidObjectId;
}

} // namespace mn::ui
