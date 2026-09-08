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

    /// Which row of its object's lane the clip is drawn on. Layout only: the
    /// exporter never reads it, so moving a clip between rows cannot change
    /// the generated Python.
    int row = 0;

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

/// A sound placed on the timeline.
///
/// Manim has no audio timeline: it takes sounds one at a time with an offset
/// from the start of the scene, which is exactly what this holds. Audio is kept
/// apart from the animation clips because it animates nothing — it has no
/// mobject, no easing, and no bearing on what the canvas draws.
struct AudioClip
{
    ClipId id = kInvalidClipId;

    /// Path to the file, relative to the project's assets folder.
    QString asset;

    /// Where it starts, in seconds from the beginning of the scene.
    double start = 0.0;

    /// Loudness adjustment in decibels, as Manim's add_sound takes it.
    double gain = 0.0;

    QJsonObject toJson() const;
    static AudioClip fromJson(const QJsonObject &object);
};

/// The absolute-time arrangement of every animation in the document.
struct Timeline
{
    QVector<Track> tracks;
    QVector<Clip> clips;
    QVector<AudioClip> audio;

    /// Explicit end of the video. Anything past it is still exported; this is
    /// the point the editor pads to with a final wait.
    double duration = 10.0;

    /// Latest point any clip reaches, ignoring `duration`.
    double contentEnd() const;

    /// Give every clip a row by stacking each object's overlapping animations,
    /// which is how lanes were laid out before rows were part of the file.
    void packRows();

    QJsonObject toJson() const;
    static Timeline fromJson(const QJsonObject &object);
};

} // namespace mn
