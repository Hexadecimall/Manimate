#pragma once

#include "SceneObject.h"
#include "Timeline.h"
#include "Types.h"

#include <QColor>
#include <QDateTime>
#include <QJsonObject>
#include <QSize>
#include <QStringList>
#include <QUuid>

namespace mn {

/// How the finished video is rendered. Mirrors the knobs Manim exposes on the
/// command line so a project can reproduce its own output exactly.
struct RenderSettings
{
    int width = 1920;
    int height = 1080;
    int fps = 60;
    QColor background = QColor(QStringLiteral("#000000"));
    bool transparent = false;

    /// Manim quality flag: "l", "m", "h", "p" or "k".
    QString quality = QStringLiteral("h");

    /// Container written into output/, e.g. "mp4".
    QString format = QStringLiteral("mp4");

    /// Manim's world is measured in frame units, not pixels. Its frame is 8
    /// units tall and as wide as the aspect ratio demands; the canvas renderer
    /// and the exporter both work in these units so previews and real renders
    /// agree on where things are.
    static constexpr double kFrameHeightUnits = 8.0;
    double frameWidthUnits() const;

    QJsonObject toJson() const;
    static RenderSettings fromJson(const QJsonObject &object);
};

/// Where the camera is looking.
///
/// Manim's ThreeDScene orients its camera with two angles: phi away from the
/// z axis, and theta round it. The canvas projects through the same two, so a
/// scene tilts on screen exactly as it will when rendered — rather than a fixed
/// isometric view that only resembles the result.
struct Camera3D
{
    /// True once the scene is three-dimensional. A flat scene keeps a flat
    /// camera, and stays a plain Scene when written out.
    bool enabled = false;

    /// Polar angle from the z axis, in degrees. 0 looks straight down it,
    /// which is the ordinary flat view.
    double phi = 0.0;

    /// Azimuth round the z axis, in degrees. Manim's own default is -90.
    double theta = -90.0;

    /// Turns the camera steadily during the scene.
    bool ambientRotation = false;
    double rotationRate = 0.2;

    /// True when this is the camera a flat scene would have anyway.
    bool isFlat() const;

    QJsonObject toJson() const;
    static Camera3D fromJson(const QJsonObject &object);
};

/// Descriptive metadata about the project. None of it affects rendering; it
/// exists so the launcher can show a meaningful project list and so a project
/// carries its own history.
struct ProjectMetadata
{
    QUuid uuid;
    QString name;
    QString description;
    QStringList tags;
    QDateTime created;
    QDateTime modified;

    /// Version of Manimation that last wrote the file.
    QString writtenBy;

    /// Number of times the project has been saved.
    int revision = 0;

    QJsonObject toJson() const;
    static ProjectMetadata fromJson(const QJsonObject &object);
};

/// The whole editable project: everything the editor knows, and the only thing
/// that needs backing up. Contents of the Manimation/ folder are derived from
/// this and can be deleted without loss.
class Document
{
public:
    Document();

    ProjectMetadata metadata;
    RenderSettings render;
    Camera3D camera;

    /// Python class name used for the exported Scene subclass.
    QString sceneClassName = QStringLiteral("MainScene");

    QVector<SceneObject> objects;
    Timeline timeline;

    /// Project-relative paths of files under assets/.
    QStringList assets;

    /// Create a document named `name`, with a fresh uuid and one empty track.
    static Document createNew(const QString &name);

    SceneObject *findObject(ObjectId id);
    const SceneObject *findObject(ObjectId id) const;
    Clip *findClip(ClipId id);
    const Clip *findClip(ClipId id) const;

    /// Add `object`, assigning it a new id and a unique name.
    ObjectId addObject(SceneObject object);
    ClipId addClip(Clip clip);

    /// Remove an object and every clip that animates it.
    bool removeObject(ObjectId id);
    bool removeClip(ClipId id);

    /// Turn `base` into a name no existing object is using.
    QString uniqueObjectName(const QString &base) const;

    QJsonObject toJson() const;
    static Document fromJson(const QJsonObject &object, QString *errorOut = nullptr);

    /// Serialise to a .manproj file. Writes to a temporary file and renames, so
    /// an interrupted save cannot destroy the previous one.
    bool save(const QString &filePath, QString *errorOut = nullptr);
    static bool load(const QString &filePath, Document *out, QString *errorOut = nullptr);

private:
    ObjectId m_nextObjectId = 1;
    ClipId m_nextClipId = 1;
};

} // namespace mn
