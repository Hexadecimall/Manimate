#pragma once

#include "Document.h"
#include "SceneEvaluator.h"

#include <QPainterPath>
#include <QRectF>
#include <QTransform>

class QPainter;

namespace mn::ui {

/// Draws a scene the way Manim would.
///
/// Everything here works in Manim's own coordinate space — a frame eight units
/// tall, y pointing up, the origin in the middle — and converts to pixels once,
/// at the end. Sharing the coordinate system with the exporter is what keeps
/// the preview and the rendered video talking about the same positions.
///
/// This is an approximation of Manim's output, not a reimplementation of it:
/// good enough to compose against at speed, with the Render button for truth.
class SceneRenderer
{
public:
    /// The largest rectangle inside `viewport` with the project's aspect ratio.
    static QRectF frameRectFor(const Document &document, const QRectF &viewport);

    /// Maps scene units to pixels within `frameRect`.
    static QTransform sceneToPixels(const Document &document, const QRectF &frameRect);

    /// Paint the frame at `time` into `frameRect`, background included.
    static void render(QPainter &painter, const Document &document, double time,
                       const QRectF &frameRect);

    /// Paint one already-evaluated object. Exposed so the canvas can draw
    /// selection decorations around exactly what it drew.
    static void renderObject(QPainter &painter, const evaluator::ObjectState &state,
                             const QTransform &toPixels);

    /// The object's outline in scene units, before its own transform.
    static QPainterPath shapeOf(const evaluator::ObjectState &state);

    /// Where the object sits in scene units, including animated motion.
    ///
    /// `shape` is the object's own outline, needed because Manim's move_to
    /// centres a mobject's bounding box rather than its local origin. For most
    /// shapes those coincide; for a triangle the circumcentre sits a quarter of
    /// a radius above the box's centre, so ignoring the difference draws it
    /// high while a square and a circle beside it sit centred.
    static QTransform transformOf(const evaluator::ObjectState &state,
                                  const QPainterPath &shape);

    /// True when a shape is defined by explicit endpoints, so its position is
    /// an offset rather than a centre.
    static bool positionIsAnOffset(const QString &type);

    /// Bounding box in pixels, for hit testing and selection handles.
    static QRectF boundsInPixels(const evaluator::ObjectState &state, const QTransform &toPixels);

    /// The topmost object at `scenePoint`, or an invalid id.
    static ObjectId objectAt(const QVector<evaluator::ObjectState> &states, const QPointF &scenePoint);

    /// Scene units per point of font size. Text is the one place where Manim's
    /// sizing does not fall out of the geometry, so it is calibrated here.
    static constexpr double kFontUnitsPerPoint = 0.0125;
};

} // namespace mn::ui
