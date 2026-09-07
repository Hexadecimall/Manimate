#pragma once

#include "Types.h"

#include <QJsonObject>
#include <QString>
#include <QVariantMap>
#include <QVector>

namespace mn {

/// A lane on the timeline. Clips reference their track by index.
struct Track
{
    QString name;
    bool enabled = true;
    bool locked = false;

    QJsonObject toJson() const;
    static Track fromJson(const QJsonObject &object);
};

/// One animation placed at an absolute time on the timeline.
///
/// Absolute placement is the editor's model, not Manim's. Manim runs a
/// sequence of play() calls with no global clock, so the exporter's solver
/// converts these absolute spans back into ordered play/wait calls. Clips can
/// therefore overlap arbitrarily here.
struct Clip
{
    ClipId id = kInvalidClipId;

    /// The object being animated.
    ObjectId objectId = kInvalidObjectId;

    /// Catalog identifier, e.g. "manim.Create".
    QString type;

    int track = 0;

    /// Absolute start on the timeline, in seconds.
    double start = 0.0;

    /// Length in seconds. Becomes run_time on export.
    double duration = 1.0;

    /// Manim rate function name, e.g. "smooth" or "linear".
    QString rateFunc = QStringLiteral("smooth");

    /// Catalog-defined parameters for this animation.
    QVariantMap params;

    /// Escape hatch: replaces the generated animation expression when set.
    QString rawPython;

    double end() const { return start + duration; }

    QJsonObject toJson() const;
    static Clip fromJson(const QJsonObject &object);
};

/// The absolute-time arrangement of every animation in the document.
struct Timeline
{
    QVector<Track> tracks;
    QVector<Clip> clips;

    /// Explicit end of the video. Anything past it is still exported; this is
    /// the point the editor pads to with a final wait.
    double duration = 10.0;

    /// Latest point any clip reaches, ignoring `duration`.
    double contentEnd() const;

    QJsonObject toJson() const;
    static Timeline fromJson(const QJsonObject &object);
};

} // namespace mn
