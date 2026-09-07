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

/// A point in the scene's own three dimensions, before the camera sees it.
struct Point3D
{
    double x = 0;
    double y = 0;
    double z = 0;
};

/// The camera a solid is being drawn for. Set while a shape is built, because
/// the geometry has to be projected as it is generated rather than afterwards.
thread_local Camera3D t_camera;

QPointF flatten(const Point3D &point)
{
    return SceneRenderer::project(t_camera, point.x, point.y, point.z);
}

void addEdges(QPainterPath &path, const QVector<Point3D> &vertices,
              const QVector<QPair<int, int>> &edges)
{
    for (const auto &edge : edges) {
        path.moveTo(flatten(vertices.at(edge.first)));
        path.lineTo(flatten(vertices.at(edge.second)));
    }
}

/// A circle lying in three dimensions, sampled and projected.
void addRing(QPainterPath &path, const Point3D &centre, double radius, int axis, int samples = 48)
{
    for (int i = 0; i <= samples; ++i) {
        const double t = 2.0 * M_PI * (double(i) / samples);
        Point3D point = centre;
        // axis names the direction the ring is perpendicular to.
        if (axis == 0) {
            point.y += radius * std::cos(t);
            point.z += radius * std::sin(t);
        } else if (axis == 1) {
            point.x += radius * std::cos(t);
            point.z += radius * std::sin(t);
        } else {
            point.x += radius * std::cos(t);
            point.y += radius * std::sin(t);
        }
        if (i == 0)
            path.moveTo(flatten(point));
        else
            path.lineTo(flatten(point));
    }
}

QPainterPath boxWireframe(double width, double height, double depth)
{
    const double hw = width / 2.0;
    const double hh = height / 2.0;
    const double hd = depth / 2.0;

    const QVector<Point3D> corners = {
        {-hw, -hh, -hd}, {hw, -hh, -hd}, {hw, hh, -hd}, {-hw, hh, -hd},
        {-hw, -hh, hd},  {hw, -hh, hd},  {hw, hh, hd},  {-hw, hh, hd},
    };
    const QVector<QPair<int, int>> edges = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                            {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

    QPainterPath path;
    addEdges(path, corners, edges);
    return path;
}

/// A sphere as a set of latitude and longitude rings, projected. Unlike a fixed
/// pair of ellipses this actually turns when the camera does.
QPainterPath sphereWireframe(double radius)
{
    QPainterPath path;
    constexpr int kRings = 4;
    for (int i = 1; i < kRings; ++i) {
        const double phi = M_PI * (double(i) / kRings);
        addRing(path, {0, 0, radius * std::cos(phi)}, radius * std::sin(phi), 2);
    }
    for (int i = 0; i < kRings; ++i) {
        const double theta = M_PI * (double(i) / kRings);
        // A meridian: a full circle tilted round the z axis.
        for (int j = 0; j <= 48; ++j) {
            const double t = 2.0 * M_PI * (double(j) / 48);
            const Point3D point{radius * std::sin(t) * std::cos(theta),
                                radius * std::sin(t) * std::sin(theta), radius * std::cos(t)};
            if (j == 0)
                path.moveTo(flatten(point));
            else
                path.lineTo(flatten(point));
        }
    }
    return path;
}

/// Vertices of a platonic solid, by face count.
QVector<Point3D> platonicVertices(int faces, double size)
{
    const double a = size / 2.0;
    switch (faces) {
    case 4:
        return {{a, a, a}, {a, -a, -a}, {-a, a, -a}, {-a, -a, a}};
    case 8:
        return {{a, 0, 0}, {-a, 0, 0}, {0, a, 0}, {0, -a, 0}, {0, 0, a}, {0, 0, -a}};
    case 20: {
        const double g = a * 1.618033988749895;   // the golden ratio
        return {{0, a, g},  {0, a, -g},  {0, -a, g},  {0, -a, -g},
                {a, g, 0},  {a, -g, 0},  {-a, g, 0},  {-a, -g, 0},
                {g, 0, a},  {-g, 0, a},  {g, 0, -a},  {-g, 0, -a}};
    }
    default: {
        // A dodecahedron's vertices: a cube plus three rectangles.
        const double g = a * 1.618033988749895;
        const double h = a / 1.618033988749895;
        QVector<Point3D> vertices;
        for (const double sx : {-a, a})
            for (const double sy : {-a, a})
                for (const double sz : {-a, a})
                    vertices.append({sx, sy, sz});
        for (const double s1 : {-h, h})
            for (const double s2 : {-g, g}) {
                vertices.append({0, s1, s2});
                vertices.append({s1, s2, 0});
                vertices.append({s2, 0, s1});
            }
        return vertices;
    }
    }
}

/// Join every pair of vertices that sit a shortest-edge apart, which draws the
/// solid's real edges without needing a face table for each one.
QPainterPath polyhedronWireframe(int faces, double size)
{
    const QVector<Point3D> vertices = platonicVertices(faces, size);
    if (vertices.size() < 2)
        return {};

    auto distance = [](const Point3D &a, const Point3D &b) {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)
                         + (a.z - b.z) * (a.z - b.z));
    };

    double shortest = std::numeric_limits<double>::max();
    for (int i = 0; i < vertices.size(); ++i) {
        for (int j = i + 1; j < vertices.size(); ++j)
            shortest = qMin(shortest, distance(vertices.at(i), vertices.at(j)));
    }

    QPainterPath path;
    for (int i = 0; i < vertices.size(); ++i) {
        for (int j = i + 1; j < vertices.size(); ++j) {
            if (distance(vertices.at(i), vertices.at(j)) <= shortest * 1.05) {
                path.moveTo(flatten(vertices.at(i)));
                path.lineTo(flatten(vertices.at(j)));
            }
        }
    }
    return path;
}

QPainterPath dashedLine(const QPointF &start, const QPointF &end, double dash)
{
    QPainterPath path;
    const QLineF line(start, end);
    const double length = line.length();
    if (length <= 0.0 || dash <= 0.0)
        return path;

    const int steps = qMax(1, int(length / (dash * 2.0)));
    for (int i = 0; i < steps; ++i) {
        const double a = double(i) * 2.0 * dash / length;
        const double b = qMin(1.0, (double(i) * 2.0 + 1.0) * dash / length);
        path.moveTo(line.pointAt(a));
        path.lineTo(line.pointAt(b));
    }
    return path;
}

QPainterPath bracePath(double length, double depth)
{
    // A curly brace, drawn as two hooks meeting at a central point.
    QPainterPath path;
    const double half = length / 2.0;
    path.moveTo(-half, 0);
    path.quadTo(-half + depth, 0, -half + depth, -depth);
    path.lineTo(-depth, -depth);
    path.quadTo(0, -depth, 0, -depth * 2.0);
    path.quadTo(0, -depth, depth, -depth);
    path.lineTo(half - depth, -depth);
    path.quadTo(half - depth, 0, half, 0);
    return path;
}

QVector<double> numberList(const QString &text)
{
    QVector<double> values;
    for (const QString &part : text.split(QRegularExpression(QStringLiteral("[,\\s]+")),
                                          Qt::SkipEmptyParts)) {
        bool ok = false;
        const double value = part.toDouble(&ok);
        if (ok)
            values.append(value);
    }
    return values;
}

QPainterPath gridOfCells(const QString &rows, double cellWidth, double cellHeight, bool brackets)
{
    const QStringList lines = rows.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    int columns = 0;
    for (const QString &line : lines)
        columns = qMax(columns, int(line.split(QLatin1Char(',')).size()));
    if (lines.isEmpty() || columns == 0)
        return {};

    const double width = columns * cellWidth;
    const double height = lines.size() * cellHeight;

    QPainterPath path;
    if (brackets) {
        // A matrix is drawn with brackets rather than a full grid.
        const double hw = width / 2.0;
        const double hh = height / 2.0;
        const double lip = cellWidth * 0.18;
        path.moveTo(-hw + lip, hh);
        path.lineTo(-hw, hh);
        path.lineTo(-hw, -hh);
        path.lineTo(-hw + lip, -hh);
        path.moveTo(hw - lip, hh);
        path.lineTo(hw, hh);
        path.lineTo(hw, -hh);
        path.lineTo(hw - lip, -hh);
        return path;
    }

    for (int c = 0; c <= columns; ++c) {
        const double x = -width / 2.0 + c * cellWidth;
        path.moveTo(x, -height / 2.0);
        path.lineTo(x, height / 2.0);
    }
    for (int r = 0; r <= lines.size(); ++r) {
        const double y = -height / 2.0 + r * cellHeight;
        path.moveTo(-width / 2.0, y);
        path.lineTo(width / 2.0, y);
    }
    return path;
}

} // namespace

QPointF SceneRenderer::project(const Camera3D &camera, double x, double y, double z)
{
    if (!camera.enabled)
        return QPointF(x, y);

    const double phi = qDegreesToRadians(camera.phi);
    const double theta = qDegreesToRadians(camera.theta);

    const double sinPhi = std::sin(phi);
    const double cosPhi = std::cos(phi);
    const double sinTheta = std::sin(theta);
    const double cosTheta = std::cos(theta);

    // Screen right and up, in world terms. At phi=0, theta=-90 these come out
    // as (1,0,0) and (0,1,0), so a flat scene is left exactly as it was.
    const double screenX = -x * sinTheta + y * cosTheta;
    const double screenY = -x * cosPhi * cosTheta - y * cosPhi * sinTheta + z * sinPhi;

    // Manim's camera is perspective, not orthographic: the axis leaning towards
    // the viewer is drawn longer than the one leaning away. Measured against a
    // render — its focal distance is 20 scene units — because a projection that
    // is merely plausible would put the canvas and the video out of step.
    const double depth = x * sinPhi * cosTheta + y * sinPhi * sinTheta + z * cosPhi;
    const double factor = SceneRenderer::kFocalDistance
                          / qMax(0.001, SceneRenderer::kFocalDistance - depth);

    return QPointF(screenX * factor, screenY * factor);
}

QTransform SceneRenderer::planeTransform(const Camera3D &camera, double z)
{
    if (!camera.enabled)
        return {};

    const double phi = qDegreesToRadians(camera.phi);
    const double theta = qDegreesToRadians(camera.theta);
    const double cosPhi = std::cos(phi);

    // Columns are where the world's x and y axes land on screen.
    return QTransform(-std::sin(theta), -cosPhi * std::cos(theta),
                      std::cos(theta), -cosPhi * std::sin(theta),
                      0.0, z * std::sin(phi));
}

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

bool SceneRenderer::isSolid(const QString &type)
{
    const catalog::MobjectSpec *spec = catalog::findMobject(type);
    if (!spec)
        return false;

    switch (spec->shape) {
    case ShapeKind::Cube:
    case ShapeKind::Sphere:
    case ShapeKind::Cone:
    case ShapeKind::Cylinder:
    case ShapeKind::Torus:
    case ShapeKind::Prism:
    case ShapeKind::Surface3D:
    case ShapeKind::ThreeDAxes:
    case ShapeKind::Line3D:
    case ShapeKind::Arrow3D:
    case ShapeKind::Dot3D:
    case ShapeKind::Polyhedron:
        return true;
    default:
        return false;
    }
}

QPainterPath SceneRenderer::shapeOf(const evaluator::ObjectState &state, const Camera3D &camera)
{
    const catalog::MobjectSpec *spec = catalog::findMobject(state.type);
    if (!spec)
        return {};

    // The builders below project as they go, so the camera has to be in place
    // before any of them runs.
    t_camera = camera;

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

    case ShapeKind::Arc: {
        const double radius = numberParam(p, QStringLiteral("radius"), 1.0);
        const double from = numberParam(p, QStringLiteral("start_angle"), 0.0);
        const double sweep = numberParam(p, QStringLiteral("angle"), 90.0);
        path.arcMoveTo(QRectF(-radius, -radius, radius * 2, radius * 2), from);
        path.arcTo(QRectF(-radius, -radius, radius * 2, radius * 2), from, sweep);
        break;
    }
    case ShapeKind::Sector: {
        const double radius = numberParam(p, QStringLiteral("radius"), 1.0);
        const double from = numberParam(p, QStringLiteral("start_angle"), 0.0);
        const double sweep = numberParam(p, QStringLiteral("angle"), 90.0);
        path.moveTo(0, 0);
        path.arcTo(QRectF(-radius, -radius, radius * 2, radius * 2), from, sweep);
        path.closeSubpath();
        break;
    }
    case ShapeKind::Annulus: {
        const double inner = numberParam(p, QStringLiteral("inner_radius"), 0.6);
        const double outer = numberParam(p, QStringLiteral("outer_radius"), 1.0);
        path.addEllipse(QPointF(0, 0), outer, outer);
        path.addEllipse(QPointF(0, 0), inner, inner);
        path.setFillRule(Qt::OddEvenFill);
        break;
    }
    case ShapeKind::Cross: {
        const double half = numberParam(p, QStringLiteral("size"), 1.0) / 2.0;
        path.moveTo(-half, -half);
        path.lineTo(half, half);
        path.moveTo(-half, half);
        path.lineTo(half, -half);
        break;
    }
    case ShapeKind::Elbow: {
        const double size = numberParam(p, QStringLiteral("width"), 0.5);
        path.moveTo(0, size);
        path.lineTo(0, 0);
        path.lineTo(size, 0);
        break;
    }
    case ShapeKind::Angle: {
        const double radius = numberParam(p, QStringLiteral("radius"), 0.5);
        const double sweep = numberParam(p, QStringLiteral("angle"), 90.0);
        path.arcMoveTo(QRectF(-radius, -radius, radius * 2, radius * 2), 0);
        path.arcTo(QRectF(-radius, -radius, radius * 2, radius * 2), 0, sweep);
        break;
    }

    case ShapeKind::DashedLine:
        path = dashedLine(pointParam(p, QStringLiteral("start")), pointParam(p, QStringLiteral("end")),
                          numberParam(p, QStringLiteral("dash_length"), 0.15));
        break;
    case ShapeKind::DoubleArrow: {
        const QPointF start = pointParam(p, QStringLiteral("start"));
        const QPointF end = pointParam(p, QStringLiteral("end"));
        const double tip = numberParam(p, QStringLiteral("tip_length"), 0.25);
        path = arrow(start, end, tip);
        path.addPath(arrow(end, start, tip));
        break;
    }
    case ShapeKind::Vector:
        path = arrow(QPointF(0, 0), pointParam(p, QStringLiteral("direction")),
                     numberParam(p, QStringLiteral("tip_length"), 0.25));
        break;
    case ShapeKind::CurvedArrow: {
        const QPointF start = pointParam(p, QStringLiteral("start"));
        const QPointF end = pointParam(p, QStringLiteral("end"));
        const double bend = numberParam(p, QStringLiteral("angle"), 45.0);
        const QPointF middle = (start + end) / 2.0;
        const QPointF away(-(end.y() - start.y()), end.x() - start.x());
        const double lift = std::tan(qDegreesToRadians(bend) / 4.0);
        path.moveTo(start);
        path.quadTo(middle + away * lift, end);
        // The tip follows the curve's own direction at the end.
        const QPointF approach = end - (middle + away * lift);
        const double angle = std::atan2(approach.y(), approach.x());
        constexpr double spread = M_PI / 7.0;
        const double tip = 0.22;
        for (const double side : {angle + M_PI - spread, angle + M_PI + spread}) {
            path.moveTo(end);
            path.lineTo(end + QPointF(tip * std::cos(side), tip * std::sin(side)));
        }
        break;
    }

    case ShapeKind::Paragraph:
        path = textPath(p.value(QStringLiteral("text")).toString(),
                        numberParam(p, QStringLiteral("font_size"), 36), false, false);
        break;
    case ShapeKind::CodeBlock: {
        path = textPath(p.value(QStringLiteral("code")).toString(),
                        numberParam(p, QStringLiteral("font_size"), 24), false, false);
        // A code block sits in a panel, which is most of what it looks like.
        const QRectF bounds = path.boundingRect().adjusted(-0.25, -0.2, 0.25, 0.2);
        QPainterPath framed;
        framed.addRoundedRect(bounds, 0.12, 0.12);
        framed.addPath(path);
        path = framed;
        break;
    }
    case ShapeKind::NumberText: {
        const int places = p.value(QStringLiteral("num_decimal_places")).toInt();
        path = textPath(QString::number(numberParam(p, QStringLiteral("number"), 0.0), 'f', places),
                        numberParam(p, QStringLiteral("font_size"), 48), false, false);
        break;
    }

    case ShapeKind::NumberLine: {
        const double length = numberParam(p, QStringLiteral("length"), 8.0);
        const double step = qMax(0.1, numberParam(p, QStringLiteral("step"), 1.0));
        const double half = length / 2.0;
        path.moveTo(-half, 0);
        path.lineTo(half, 0);
        for (double x = -half; x <= half + 1e-6; x += step) {
            path.moveTo(x, -0.12);
            path.lineTo(x, 0.12);
        }
        break;
    }
    case ShapeKind::BarChart: {
        const QVector<double> values = numberList(p.value(QStringLiteral("values")).toString());
        const double top = qMax(0.001, numberParam(p, QStringLiteral("y_range"), 6.0));
        if (values.isEmpty())
            break;
        const double barWidth = 0.6;
        const double gap = 0.25;
        const double total = values.size() * barWidth + (values.size() - 1) * gap;
        double x = -total / 2.0;
        for (const double value : values) {
            const double height = qBound(0.0, value / top, 1.0) * 3.0;
            path.addRect(QRectF(x, 0, barWidth, height));
            x += barWidth + gap;
        }
        break;
    }
    case ShapeKind::FunctionGraph: {
        // The expression is not evaluated here; a representative curve stands
        // in for it until the render, which is the thing that knows Python.
        const double from = numberParam(p, QStringLiteral("x_min"), -4.0);
        const double to = numberParam(p, QStringLiteral("x_max"), 4.0);
        constexpr int kSamples = 96;
        for (int i = 0; i <= kSamples; ++i) {
            const double x = from + (to - from) * (double(i) / kSamples);
            const QPointF point(x, std::sin(x));
            if (i == 0)
                path.moveTo(point);
            else
                path.lineTo(point);
        }
        break;
    }
    case ShapeKind::Table:
        path = gridOfCells(p.value(QStringLiteral("rows")).toString(),
                           numberParam(p, QStringLiteral("cell_width"), 1.2),
                           numberParam(p, QStringLiteral("cell_height"), 0.8), false);
        break;
    case ShapeKind::Matrix:
        path = gridOfCells(p.value(QStringLiteral("rows")).toString(),
                           numberParam(p, QStringLiteral("cell_width"), 0.8),
                           numberParam(p, QStringLiteral("cell_height"), 0.7), true);
        break;
    case ShapeKind::Brace:
        path = bracePath(numberParam(p, QStringLiteral("length"), 2.0),
                         numberParam(p, QStringLiteral("depth"), 0.3));
        break;
    case ShapeKind::Underline: {
        const double half = numberParam(p, QStringLiteral("length"), 2.0) / 2.0;
        path.moveTo(-half, 0);
        path.lineTo(half, 0);
        break;
    }
    case ShapeKind::SurroundingBox: {
        const double buff = numberParam(p, QStringLiteral("buff"), 0.1);
        const double width = numberParam(p, QStringLiteral("width"), 2.0) + buff * 2;
        const double height = numberParam(p, QStringLiteral("height"), 1.0) + buff * 2;
        path.addRect(-width / 2.0, -height / 2.0, width, height);
        break;
    }

    case ShapeKind::Cube: {
        const double side = numberParam(p, QStringLiteral("side_length"), 2.0);
        path = boxWireframe(side, side, side);
        break;
    }
    case ShapeKind::Prism:
        path = boxWireframe(numberParam(p, QStringLiteral("width"), 2.0),
                            numberParam(p, QStringLiteral("height"), 1.0),
                            numberParam(p, QStringLiteral("depth"), 1.0));
        break;
    case ShapeKind::Sphere:
        path = sphereWireframe(numberParam(p, QStringLiteral("radius"), 1.0));
        break;
    case ShapeKind::Cone: {
        const double radius = numberParam(p, QStringLiteral("base_radius"), 1.0);
        const double height = numberParam(p, QStringLiteral("height"), 2.0);
        addRing(path, {0, 0, -height / 2.0}, radius, 2);
        const Point3D apex{0, 0, height / 2.0};
        for (int i = 0; i < 8; ++i) {
            const double t = 2.0 * M_PI * (double(i) / 8);
            path.moveTo(flatten({radius * std::cos(t), radius * std::sin(t), -height / 2.0}));
            path.lineTo(flatten(apex));
        }
        break;
    }
    case ShapeKind::Cylinder: {
        const double radius = numberParam(p, QStringLiteral("radius"), 1.0);
        const double height = numberParam(p, QStringLiteral("height"), 2.0);
        addRing(path, {0, 0, height / 2.0}, radius, 2);
        addRing(path, {0, 0, -height / 2.0}, radius, 2);
        for (int i = 0; i < 8; ++i) {
            const double t = 2.0 * M_PI * (double(i) / 8);
            const double x = radius * std::cos(t);
            const double y = radius * std::sin(t);
            path.moveTo(flatten({x, y, -height / 2.0}));
            path.lineTo(flatten({x, y, height / 2.0}));
        }
        break;
    }
    case ShapeKind::Torus: {
        const double major = numberParam(p, QStringLiteral("major_radius"), 1.0);
        const double minor = numberParam(p, QStringLiteral("minor_radius"), 0.35);
        constexpr int kRings = 10;
        for (int i = 0; i < kRings; ++i) {
            const double t = 2.0 * M_PI * (double(i) / kRings);
            // Each ring of the tube, standing where the major circle puts it.
            for (int j = 0; j <= 24; ++j) {
                const double u = 2.0 * M_PI * (double(j) / 24);
                const double r = major + minor * std::cos(u);
                const Point3D point{r * std::cos(t), r * std::sin(t), minor * std::sin(u)};
                if (j == 0)
                    path.moveTo(flatten(point));
                else
                    path.lineTo(flatten(point));
            }
        }
        addRing(path, {0, 0, 0}, major + minor, 2);
        addRing(path, {0, 0, 0}, major - minor, 2);
        break;
    }
    case ShapeKind::Surface3D: {
        const double extent = numberParam(p, QStringLiteral("extent"), 2.0);
        constexpr int kLines = 9;
        auto height = [](double u, double v) { return std::sin(u) * std::cos(v) * 0.6; };
        for (int i = 0; i < kLines; ++i) {
            const double u = -extent + 2 * extent * (double(i) / (kLines - 1));
            for (int j = 0; j < kLines; ++j) {
                const double v = -extent + 2 * extent * (double(j) / (kLines - 1));
                const QPointF screen = flatten({u, v, height(u, v)});
                if (j == 0)
                    path.moveTo(screen);
                else
                    path.lineTo(screen);
            }
        }
        for (int j = 0; j < kLines; ++j) {
            const double v = -extent + 2 * extent * (double(j) / (kLines - 1));
            for (int i = 0; i < kLines; ++i) {
                const double u = -extent + 2 * extent * (double(i) / (kLines - 1));
                const QPointF screen = flatten({u, v, height(u, v)});
                if (i == 0)
                    path.moveTo(screen);
                else
                    path.lineTo(screen);
            }
        }
        break;
    }

    case ShapeKind::ThreeDAxes: {
        const double extent = numberParam(p, QStringLiteral("extent"), 3.0);
        const Point3D origin{0, 0, 0};
        for (const Point3D &tip : {Point3D{extent, 0, 0}, Point3D{0, extent, 0},
                                   Point3D{0, 0, extent}}) {
            path.moveTo(flatten({-tip.x, -tip.y, -tip.z}));
            path.lineTo(flatten(tip));
            // A short cap, so the far end of each axis is visible.
            const Point3D cap{tip.x * 0.9, tip.y * 0.9, tip.z * 0.9};
            path.moveTo(flatten(cap));
            path.lineTo(flatten(tip));
        }
        Q_UNUSED(origin);
        break;
    }
    case ShapeKind::Line3D: {
        const QPointF start = pointParam(p, QStringLiteral("start"));
        const QPointF end = pointParam(p, QStringLiteral("end"));
        path.moveTo(flatten({start.x(), start.y(), numberParam(p, QStringLiteral("start_z"), 0.0)}));
        path.lineTo(flatten({end.x(), end.y(), numberParam(p, QStringLiteral("end_z"), 1.0)}));
        break;
    }
    case ShapeKind::Arrow3D: {
        const QPointF start = pointParam(p, QStringLiteral("start"));
        const QPointF end = pointParam(p, QStringLiteral("end"));
        const QPointF a = flatten({start.x(), start.y(),
                                   numberParam(p, QStringLiteral("start_z"), 0.0)});
        const QPointF b = flatten({end.x(), end.y(), numberParam(p, QStringLiteral("end_z"), 1.0)});
        // The head is drawn on the projected line, which is where it is seen.
        path = arrow(a, b, 0.22);
        break;
    }
    case ShapeKind::Dot3D:
        addRing(path, {0, 0, 0}, numberParam(p, QStringLiteral("radius"), 0.1), 2);
        break;
    case ShapeKind::Polyhedron:
        path = polyhedronWireframe(p.value(QStringLiteral("faces")).toInt(),
                                   numberParam(p, QStringLiteral("edge_length"), 1.5));
        break;

    case ShapeKind::Group:
        // Drawn by its children; a group has no outline of its own.
        break;

    case ShapeKind::Custom: {
        // Hand-written Python cannot be drawn without running it, so the canvas
        // shows a labelled placeholder and the render shows the truth.
        const QString label = p.value(QStringLiteral("label")).toString();
        QPainterPath box;
        box.addRoundedRect(QRectF(-0.9, -0.45, 1.8, 0.9), 0.12, 0.12);
        path = box;
        QPainterPath caption = textPath(label.isEmpty() ? QStringLiteral("Custom") : label, 22,
                                        false, false);
        path.addPath(caption);
        break;
    }
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
                                      const QPainterPath &shape, const Camera3D &camera)
{
    const QPointF position = state.params.value(QStringLiteral("position")).toPointF() + state.offset;
    const double rotation = numberParam(state.params, QStringLiteral("rotation"), 0.0)
                            + state.rotationDegrees;
    const double scale = numberParam(state.params, QStringLiteral("scale"), 1.0) * state.scale;

    QTransform transform;

    // A flat shape is placed in the scene's plane and then projected; a solid
    // was projected as it was built, so it is placed on screen directly.
    if (camera.enabled && !isSolid(state.type)) {
        const double z = numberParam(state.params, QStringLiteral("z"), 0.0);
        const QPointF placed = project(camera, position.x(), position.y(), z);
        transform.translate(placed.x(), placed.y());
        transform.rotate(rotation);
        transform.scale(scale, scale);
        transform = planeTransform(camera, 0.0) * transform;
    } else if (camera.enabled) {
        const double z = numberParam(state.params, QStringLiteral("z"), 0.0);
        const QPointF placed = project(camera, position.x(), position.y(), z);
        transform.translate(placed.x(), placed.y());
        transform.rotate(rotation);
        transform.scale(scale, scale);
    } else {
        transform.translate(position.x(), position.y());
        transform.rotate(rotation);
        transform.scale(scale, scale);
    }

    // Bring the bounding box's centre to the origin first, so the position
    // names the object's centre and rotation turns about it, as in Manim.
    if (!positionIsAnOffset(state.type) && !shape.isEmpty()) {
        const QPointF centre = shape.boundingRect().center();
        transform.translate(-centre.x(), -centre.y());
    }
    return transform;
}

void SceneRenderer::renderObject(QPainter &painter, const evaluator::ObjectState &state,
                                 const QTransform &toPixels, const Camera3D &camera)
{
    QPainterPath path = shapeOf(state, camera);
    if (path.isEmpty())
        return;

    if (state.drawProgress < 1.0)
        path = partialPath(path, state.drawProgress);

    const QTransform full = transformOf(state, shapeOf(state, camera), camera) * toPixels;
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
        renderObject(painter, state, toPixels, document.camera);

    painter.restore();
}

QRectF SceneRenderer::boundsInPixels(const evaluator::ObjectState &state, const QTransform &toPixels,
                                     const Camera3D &camera)
{
    const QPainterPath path = shapeOf(state, camera);
    if (path.isEmpty())
        return {};
    return (transformOf(state, path, camera) * toPixels).map(path).boundingRect();
}

ObjectId SceneRenderer::objectAt(const QVector<evaluator::ObjectState> &states,
                                 const QPointF &scenePoint, const Camera3D &camera)
{
    // Topmost first, so clicking picks what the eye picks.
    for (int i = states.size() - 1; i >= 0; --i) {
        const evaluator::ObjectState &state = states.at(i);
        QPainterPath path = shapeOf(state, camera);
        if (path.isEmpty())
            continue;

        const QPainterPath placed = transformOf(state, path, camera).map(path);
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
