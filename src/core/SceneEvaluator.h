#pragma once

#include "Types.h"

#include <QColor>
#include <QPointF>
#include <QVariantMap>
#include <QVector>

namespace mn {

class Document;

/// Working out what a scene looks like at one instant.
///
/// The timeline stores animations at absolute times; this turns that into the
/// state of every object at a given moment, which is all the canvas needs to
/// draw a frame. The same reasoning will drive the exporter, so the preview and
/// the rendered video agree about what happens when.
namespace evaluator {

/// One object, as it appears at a particular time.
struct ObjectState
{
    ObjectId id = kInvalidObjectId;
    QString type;
    QString name;

    /// The object's parameters, with catalog defaults filled in.
    QVariantMap params;

    bool visible = true;

    /// Multiplies whatever opacity the parameters ask for.
    double opacity = 1.0;

    /// How much of the outline has been drawn, for Create and Write.
    double drawProgress = 1.0;

    /// Accumulated motion, in Manim's frame units.
    QPointF offset;
    double rotationDegrees = 0.0;
    double scale = 1.0;

    /// Set when an animation has recoloured the object.
    QColor colorOverride;

    int zOrder = 0;
};

/// Every visible object at `time`, in paint order.
QVector<ObjectState> evaluate(const Document &document, double time);

/// The state of one object at `time`, whether or not it is visible.
ObjectState evaluateObject(const Document &document, ObjectId id, double time);

} // namespace evaluator
} // namespace mn
