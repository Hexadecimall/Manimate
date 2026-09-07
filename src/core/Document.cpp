#include "Document.h"

#include "Version.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>

namespace mn {
namespace {

constexpr auto kFormatKey = "formatVersion";

} // namespace

double RenderSettings::frameWidthUnits() const
{
    if (height <= 0)
        return kFrameHeightUnits * 16.0 / 9.0;
    return kFrameHeightUnits * double(width) / double(height);
}

QJsonObject RenderSettings::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("width"), width);
    object.insert(QStringLiteral("height"), height);
    object.insert(QStringLiteral("fps"), fps);
    object.insert(QStringLiteral("background"), background.name(QColor::HexArgb));
    object.insert(QStringLiteral("transparent"), transparent);
    object.insert(QStringLiteral("quality"), quality);
    object.insert(QStringLiteral("format"), format);
    return object;
}

RenderSettings RenderSettings::fromJson(const QJsonObject &object)
{
    RenderSettings settings;
    settings.width = object.value(QStringLiteral("width")).toInt(1920);
    settings.height = object.value(QStringLiteral("height")).toInt(1080);
    settings.fps = object.value(QStringLiteral("fps")).toInt(60);
    const QString background = object.value(QStringLiteral("background")).toString();
    if (!background.isEmpty())
        settings.background = QColor::fromString(background);
    settings.transparent = object.value(QStringLiteral("transparent")).toBool(false);
    settings.quality = object.value(QStringLiteral("quality")).toString(QStringLiteral("h"));
    settings.format = object.value(QStringLiteral("format")).toString(QStringLiteral("mp4"));
    return settings;
}

bool Camera3D::isFlat() const
{
    return !enabled || (qFuzzyIsNull(phi) && !ambientRotation);
}

QJsonObject Camera3D::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("enabled"), enabled);
    object.insert(QStringLiteral("phi"), phi);
    object.insert(QStringLiteral("theta"), theta);
    object.insert(QStringLiteral("ambientRotation"), ambientRotation);
    object.insert(QStringLiteral("rotationRate"), rotationRate);
    return object;
}

Camera3D Camera3D::fromJson(const QJsonObject &object)
{
    Camera3D camera;
    camera.enabled = object.value(QStringLiteral("enabled")).toBool(false);
    camera.phi = object.value(QStringLiteral("phi")).toDouble(0.0);
    camera.theta = object.value(QStringLiteral("theta")).toDouble(-90.0);
    camera.ambientRotation = object.value(QStringLiteral("ambientRotation")).toBool(false);
    camera.rotationRate = object.value(QStringLiteral("rotationRate")).toDouble(0.2);
    return camera;
}

QJsonObject ProjectMetadata::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("uuid"), uuid.toString(QUuid::WithoutBraces));
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("description"), description);
    object.insert(QStringLiteral("tags"), QJsonArray::fromStringList(tags));
    object.insert(QStringLiteral("created"), created.toString(Qt::ISODate));
    object.insert(QStringLiteral("modified"), modified.toString(Qt::ISODate));
    object.insert(QStringLiteral("writtenBy"), writtenBy);
    object.insert(QStringLiteral("revision"), revision);
    return object;
}

ProjectMetadata ProjectMetadata::fromJson(const QJsonObject &object)
{
    ProjectMetadata metadata;
    metadata.uuid = QUuid::fromString(object.value(QStringLiteral("uuid")).toString());
    metadata.name = object.value(QStringLiteral("name")).toString();
    metadata.description = object.value(QStringLiteral("description")).toString();
    const QJsonArray tags = object.value(QStringLiteral("tags")).toArray();
    for (const QJsonValue &tag : tags)
        metadata.tags.append(tag.toString());
    metadata.created = QDateTime::fromString(object.value(QStringLiteral("created")).toString(), Qt::ISODate);
    metadata.modified = QDateTime::fromString(object.value(QStringLiteral("modified")).toString(), Qt::ISODate);
    metadata.writtenBy = object.value(QStringLiteral("writtenBy")).toString();
    metadata.revision = object.value(QStringLiteral("revision")).toInt();
    return metadata;
}

Document::Document() = default;

Document Document::createNew(const QString &name)
{
    Document document;
    document.metadata.uuid = QUuid::createUuid();
    document.metadata.name = name;
    document.metadata.created = QDateTime::currentDateTimeUtc();
    document.metadata.modified = document.metadata.created;
    document.metadata.writtenBy = version::string();

    Track track;
    track.name = QStringLiteral("Track 1");
    document.timeline.tracks.append(track);
    return document;
}

SceneObject *Document::findObject(ObjectId id)
{
    for (SceneObject &object : objects) {
        if (object.id == id)
            return &object;
    }
    return nullptr;
}

const SceneObject *Document::findObject(ObjectId id) const
{
    return const_cast<Document *>(this)->findObject(id);
}

Clip *Document::findClip(ClipId id)
{
    for (Clip &clip : timeline.clips) {
        if (clip.id == id)
            return &clip;
    }
    return nullptr;
}

const Clip *Document::findClip(ClipId id) const
{
    return const_cast<Document *>(this)->findClip(id);
}

QString Document::uniqueObjectName(const QString &base) const
{
    const QString trimmed = base.trimmed().isEmpty() ? QStringLiteral("Object") : base.trimmed();

    auto taken = [this](const QString &candidate) {
        for (const SceneObject &object : objects) {
            if (object.name.compare(candidate, Qt::CaseInsensitive) == 0)
                return true;
        }
        return false;
    };

    if (!taken(trimmed))
        return trimmed;

    for (int suffix = 2;; ++suffix) {
        const QString candidate = QStringLiteral("%1 %2").arg(trimmed).arg(suffix);
        if (!taken(candidate))
            return candidate;
    }
}

ObjectId Document::addObject(SceneObject object)
{
    object.id = m_nextObjectId++;
    object.name = uniqueObjectName(object.name);
    objects.append(std::move(object));
    return objects.back().id;
}

ClipId Document::addClip(Clip clip)
{
    clip.id = m_nextClipId++;
    timeline.clips.append(std::move(clip));
    return timeline.clips.back().id;
}

bool Document::removeObject(ObjectId id)
{
    const qsizetype removed = objects.removeIf([id](const SceneObject &object) {
        return object.id == id;
    });
    if (removed == 0)
        return false;

    timeline.clips.removeIf([id](const Clip &clip) { return clip.objectId == id; });
    for (SceneObject &object : objects) {
        if (object.parentId == id)
            object.parentId = kInvalidObjectId;
    }
    return true;
}

bool Document::removeClip(ClipId id)
{
    return timeline.clips.removeIf([id](const Clip &clip) { return clip.id == id; }) > 0;
}

QJsonObject Document::toJson() const
{
    QJsonArray objectArray;
    for (const SceneObject &object : objects)
        objectArray.append(object.toJson());

    QJsonObject root;
    root.insert(QLatin1String(kFormatKey), version::kProjectFormat);
    root.insert(QStringLiteral("metadata"), metadata.toJson());
    root.insert(QStringLiteral("render"), render.toJson());
    root.insert(QStringLiteral("camera"), camera.toJson());
    root.insert(QStringLiteral("sceneClass"), sceneClassName);
    root.insert(QStringLiteral("objects"), objectArray);
    root.insert(QStringLiteral("timeline"), timeline.toJson());
    root.insert(QStringLiteral("assets"), QJsonArray::fromStringList(assets));
    root.insert(QStringLiteral("nextObjectId"), qint64(m_nextObjectId));
    root.insert(QStringLiteral("nextClipId"), qint64(m_nextClipId));
    return root;
}

Document Document::fromJson(const QJsonObject &object, QString *errorOut)
{
    Document document;

    const int format = object.value(QLatin1String(kFormatKey)).toInt(-1);
    if (format < 0) {
        if (errorOut)
            *errorOut = QStringLiteral("Not a Manimate project: missing format version.");
        return document;
    }
    if (format > version::kProjectFormat && errorOut) {
        *errorOut = QStringLiteral(
                        "This project was made with a newer version of Manimate "
                        "(format %1, this build understands %2).")
                        .arg(format)
                        .arg(version::kProjectFormat);
        return document;
    }

    document.metadata = ProjectMetadata::fromJson(object.value(QStringLiteral("metadata")).toObject());
    document.render = RenderSettings::fromJson(object.value(QStringLiteral("render")).toObject());
    document.camera = Camera3D::fromJson(object.value(QStringLiteral("camera")).toObject());
    document.sceneClassName = object.value(QStringLiteral("sceneClass")).toString(QStringLiteral("MainScene"));

    const QJsonArray objectArray = object.value(QStringLiteral("objects")).toArray();
    document.objects.reserve(objectArray.size());
    for (const QJsonValue &value : objectArray)
        document.objects.append(SceneObject::fromJson(value.toObject()));

    document.timeline = Timeline::fromJson(object.value(QStringLiteral("timeline")).toObject());

    const QJsonArray assetArray = object.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &value : assetArray)
        document.assets.append(value.toString());

    document.m_nextObjectId = ObjectId(object.value(QStringLiteral("nextObjectId")).toInteger(1));
    document.m_nextClipId = ClipId(object.value(QStringLiteral("nextClipId")).toInteger(1));

    // Ids must never be reissued, even if the file understated them.
    for (const SceneObject &sceneObject : document.objects)
        document.m_nextObjectId = std::max(document.m_nextObjectId, sceneObject.id + 1);
    for (const Clip &clip : document.timeline.clips)
        document.m_nextClipId = std::max(document.m_nextClipId, clip.id + 1);

    return document;
}

bool Document::save(const QString &filePath, QString *errorOut)
{
    metadata.modified = QDateTime::currentDateTimeUtc();
    metadata.writtenBy = version::string();
    metadata.revision += 1;

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }

    const QJsonDocument json(toJson());
    if (file.write(json.toJson(QJsonDocument::Indented)) < 0) {
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }
    if (!file.commit()) {
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }
    return true;
}

bool Document::load(const QString &filePath, Document *out, QString *errorOut)
{
    if (!out)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorOut)
            *errorOut = QStringLiteral("%1 (offset %2)").arg(parseError.errorString()).arg(parseError.offset);
        return false;
    }
    if (!json.isObject()) {
        if (errorOut)
            *errorOut = QStringLiteral("Not a Manimate project: the file is not a JSON object.");
        return false;
    }

    QString error;
    Document document = Document::fromJson(json.object(), &error);
    if (!error.isEmpty()) {
        if (errorOut)
            *errorOut = error;
        return false;
    }

    *out = std::move(document);
    return true;
}

} // namespace mn
