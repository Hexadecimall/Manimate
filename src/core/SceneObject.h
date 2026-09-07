#pragma once

#include "Types.h"

#include <QJsonObject>
#include <QString>
#include <QVariantMap>

namespace mn {

/// One thing that exists in the scene: a circle, a piece of text, an image.
///
/// A SceneObject stores no drawing or code-generation logic of its own. `type`
/// names an entry in the catalog, and the catalog decides how the object is
/// painted on the canvas and what Python it becomes. That indirection is what
/// lets new Manim classes be added without touching this struct.
struct SceneObject
{
    ObjectId id = kInvalidObjectId;

    /// Catalog identifier, e.g. "manim.Circle".
    QString type;

    /// User-facing name, also used as the Python variable name on export.
    QString name;

    /// Parent group, or kInvalidObjectId for a top-level object.
    ObjectId parentId = kInvalidObjectId;

    /// Catalog-defined parameters: radius, colour, text, and so on.
    QVariantMap params;

    /// Paint order within a parent. Higher draws in front.
    int zOrder = 0;

    bool visible = true;
    bool locked = false;

    /// Escape hatch: emitted verbatim after the object's own constructor line.
    QString rawPython;

    QJsonObject toJson() const;
    static SceneObject fromJson(const QJsonObject &object);
};

} // namespace mn
