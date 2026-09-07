#include "Timeline.h"

#include "Json.h"

#include <QJsonArray>
#include <algorithm>

namespace mn {

QJsonObject Track::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("enabled"), enabled);
    object.insert(QStringLiteral("locked"), locked);
    return object;
}

Track Track::fromJson(const QJsonObject &object)
{
    Track track;
    track.name = object.value(QStringLiteral("name")).toString();
    track.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    track.locked = object.value(QStringLiteral("locked")).toBool(false);
    return track;
}

QJsonObject Clip::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), qint64(id));
    object.insert(QStringLiteral("object"), qint64(objectId));
    object.insert(QStringLiteral("type"), type);
    object.insert(QStringLiteral("track"), track);
    object.insert(QStringLiteral("start"), start);
    object.insert(QStringLiteral("duration"), duration);
    object.insert(QStringLiteral("rate"), rateFunc);
    object.insert(QStringLiteral("params"), json::encodeMap(params));
    if (!rawPython.isEmpty())
        object.insert(QStringLiteral("raw"), rawPython);
    return object;
}

Clip Clip::fromJson(const QJsonObject &object)
{
    Clip clip;
    clip.id = ClipId(object.value(QStringLiteral("id")).toInteger());
    clip.objectId = ObjectId(object.value(QStringLiteral("object")).toInteger());
    clip.type = object.value(QStringLiteral("type")).toString();
    clip.track = object.value(QStringLiteral("track")).toInt();
    clip.start = object.value(QStringLiteral("start")).toDouble();
    clip.duration = object.value(QStringLiteral("duration")).toDouble(1.0);
    clip.rateFunc = object.value(QStringLiteral("rate")).toString(QStringLiteral("smooth"));
    clip.params = json::decodeMap(object.value(QStringLiteral("params")).toObject());
    clip.rawPython = object.value(QStringLiteral("raw")).toString();
    return clip;
}

QJsonObject AudioClip::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), qint64(id));
    object.insert(QStringLiteral("asset"), asset);
    object.insert(QStringLiteral("start"), start);
    object.insert(QStringLiteral("gain"), gain);
    return object;
}

AudioClip AudioClip::fromJson(const QJsonObject &object)
{
    AudioClip clip;
    clip.id = ClipId(object.value(QStringLiteral("id")).toInteger());
    clip.asset = object.value(QStringLiteral("asset")).toString();
    clip.start = object.value(QStringLiteral("start")).toDouble();
    clip.gain = object.value(QStringLiteral("gain")).toDouble();
    return clip;
}

double Timeline::contentEnd() const
{
    double end = 0.0;
    for (const Clip &clip : clips)
        end = std::max(end, clip.end());
    for (const AudioClip &clip : audio)
        end = std::max(end, clip.start);
    return end;
}

QJsonObject Timeline::toJson() const
{
    QJsonArray trackArray;
    for (const Track &track : tracks)
        trackArray.append(track.toJson());

    QJsonArray clipArray;
    for (const Clip &clip : clips)
        clipArray.append(clip.toJson());

    QJsonArray audioArray;
    for (const AudioClip &clip : audio)
        audioArray.append(clip.toJson());

    QJsonObject object;
    object.insert(QStringLiteral("tracks"), trackArray);
    object.insert(QStringLiteral("clips"), clipArray);
    object.insert(QStringLiteral("audio"), audioArray);
    object.insert(QStringLiteral("duration"), duration);
    return object;
}

Timeline Timeline::fromJson(const QJsonObject &object)
{
    Timeline timeline;
    const QJsonArray trackArray = object.value(QStringLiteral("tracks")).toArray();
    timeline.tracks.reserve(trackArray.size());
    for (const QJsonValue &value : trackArray)
        timeline.tracks.append(Track::fromJson(value.toObject()));

    const QJsonArray clipArray = object.value(QStringLiteral("clips")).toArray();
    timeline.clips.reserve(clipArray.size());
    for (const QJsonValue &value : clipArray)
        timeline.clips.append(Clip::fromJson(value.toObject()));

    const QJsonArray audioArray = object.value(QStringLiteral("audio")).toArray();
    timeline.audio.reserve(audioArray.size());
    for (const QJsonValue &value : audioArray)
        timeline.audio.append(AudioClip::fromJson(value.toObject()));

    timeline.duration = object.value(QStringLiteral("duration")).toDouble(10.0);
    return timeline;
}

} // namespace mn
