#include "EditorState.h"

#include "Catalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace mn::ui {

EditorState::EditorState(QObject *parent)
    : QObject(parent)
{
}

void EditorState::setDocument(Document document, const ProjectLayout &layout)
{
    m_document = std::move(document);
    m_layout = layout;
    m_selectedObject = kInvalidObjectId;
    m_selectedClip = kInvalidClipId;
    m_playhead = 0.0;
    m_undo.clear();
    m_redo.clear();
    setModified(false);

    Q_EMIT documentChanged();
    Q_EMIT selectionChanged();
    Q_EMIT playheadChanged(m_playhead);
    Q_EMIT historyChanged();
}

void EditorState::setModified(bool modified)
{
    if (m_modified == modified)
        return;
    m_modified = modified;
    Q_EMIT modifiedChanged(m_modified);
}

double EditorState::timelineDuration() const
{
    return qMax(m_document.timeline.duration, m_document.timeline.contentEnd());
}

void EditorState::beginEdit()
{
    m_undo.append(m_document.toJson());
    if (m_undo.size() > kMaxHistory)
        m_undo.removeFirst();
    m_redo.clear();
    Q_EMIT historyChanged();
}

void EditorState::commit()
{
    setModified(true);
    Q_EMIT documentChanged();
}

void EditorState::undo()
{
    if (m_undo.isEmpty())
        return;

    m_redo.append(m_document.toJson());
    m_document = Document::fromJson(m_undo.takeLast());

    // The selection may name something that no longer exists.
    if (!m_document.findObject(m_selectedObject))
        m_selectedObject = kInvalidObjectId;
    if (!m_document.findClip(m_selectedClip))
        m_selectedClip = kInvalidClipId;

    commit();
    Q_EMIT selectionChanged();
    Q_EMIT historyChanged();
}

void EditorState::redo()
{
    if (m_redo.isEmpty())
        return;

    m_undo.append(m_document.toJson());
    m_document = Document::fromJson(m_redo.takeLast());

    if (!m_document.findObject(m_selectedObject))
        m_selectedObject = kInvalidObjectId;
    if (!m_document.findClip(m_selectedClip))
        m_selectedClip = kInvalidClipId;

    commit();
    Q_EMIT selectionChanged();
    Q_EMIT historyChanged();
}

void EditorState::selectObject(ObjectId id)
{
    if (m_selectedObject == id && m_selectedClip == kInvalidClipId)
        return;
    m_selectedObject = id;
    m_selectedClip = kInvalidClipId;
    Q_EMIT selectionChanged();
}

void EditorState::selectClip(ClipId id)
{
    const Clip *clip = m_document.findClip(id);
    if (m_selectedClip == id)
        return;

    m_selectedClip = id;
    m_selectedAudio = kInvalidClipId;
    // Selecting a clip also selects what it animates, so the inspector and the
    // canvas agree about what is being worked on.
    m_selectedObject = clip ? clip->objectId : kInvalidObjectId;
    Q_EMIT selectionChanged();
}

void EditorState::selectAudio(ClipId id)
{
    if (m_selectedAudio == id)
        return;
    m_selectedAudio = id;

    // Selecting a sound deselects the animation clip, so the inspector shows
    // one thing rather than two.
    if (id != kInvalidClipId)
        m_selectedClip = kInvalidClipId;
    Q_EMIT selectionChanged();
}

void EditorState::clearSelection()
{
    if (m_selectedObject == kInvalidObjectId && m_selectedClip == kInvalidClipId)
        return;
    m_selectedObject = kInvalidObjectId;
    m_selectedClip = kInvalidClipId;
    m_selectedAudio = kInvalidClipId;
    Q_EMIT selectionChanged();
}

void EditorState::setPlayhead(double seconds)
{
    const double clamped = std::clamp(seconds, 0.0, timelineDuration());
    if (qFuzzyCompare(m_playhead + 1.0, clamped + 1.0))
        return;
    m_playhead = clamped;
    Q_EMIT playheadChanged(m_playhead);
}

ObjectId EditorState::addObject(const QString &specId)
{
    const catalog::MobjectSpec *spec = catalog::findMobject(specId);
    if (!spec)
        return kInvalidObjectId;

    beginEdit();

    SceneObject object;
    object.type = spec->id;
    object.name = spec->defaultObjectName;
    object.params = catalog::defaultParams(*spec);

    int highest = 0;
    for (const SceneObject &existing : m_document.objects)
        highest = qMax(highest, existing.zOrder);
    object.zOrder = highest + 1;

    const ObjectId id = m_document.addObject(std::move(object));

    commit();
    selectObject(id);
    return id;
}

int EditorState::freeTrackFor(double start, double duration) const
{
    const double end = start + duration;

    // Reuse the first track with room, so clips do not pile into new lanes.
    for (int track = 0; track < m_document.timeline.tracks.size(); ++track) {
        bool clashes = false;
        for (const Clip &clip : m_document.timeline.clips) {
            if (clip.track != track)
                continue;
            if (start < clip.end() && clip.start < end) {
                clashes = true;
                break;
            }
        }
        if (!clashes)
            return track;
    }
    return int(m_document.timeline.tracks.size());
}

ClipId EditorState::addClip(ObjectId objectId, const QString &animationId)
{
    const catalog::AnimationSpec *spec = catalog::findAnimation(animationId);
    if (!spec || !m_document.findObject(objectId))
        return kInvalidClipId;

    beginEdit();

    // The timeline lays out one lane per object, so a clip needs no track of
    // its own: it appears under whatever it animates.
    Clip clip;
    clip.objectId = objectId;
    clip.type = spec->id;
    clip.start = m_playhead;
    clip.duration = spec->defaultDuration;
    clip.params = catalog::defaultParams(*spec);

    const ClipId id = m_document.addClip(std::move(clip));

    commit();
    selectClip(id);
    return id;
}

ObjectId EditorState::groupObjects(const QVector<ObjectId> &ids)
{
    QVector<ObjectId> members;
    for (const ObjectId id : ids) {
        if (m_document.findObject(id))
            members.append(id);
    }
    if (members.size() < 2)
        return kInvalidObjectId;

    beginEdit();

    SceneObject group;
    group.type = QStringLiteral("manim.VGroup");
    group.name = QStringLiteral("Group");
    group.params = catalog::defaultParams(*catalog::findMobject(group.type));

    int highest = 0;
    for (const SceneObject &existing : m_document.objects)
        highest = qMax(highest, existing.zOrder);
    group.zOrder = highest + 1;

    const ObjectId groupId = m_document.addObject(std::move(group));

    // The members keep the positions they already have: the group starts at the
    // origin so nothing moves the moment it is created.
    for (const ObjectId id : members) {
        if (SceneObject *object = m_document.findObject(id))
            object->parentId = groupId;
    }

    commit();
    selectObject(groupId);
    return groupId;
}

void EditorState::ungroupObject(ObjectId id)
{
    SceneObject *object = m_document.findObject(id);
    if (!object || object->parentId == kInvalidObjectId)
        return;

    const SceneObject *parent = m_document.findObject(object->parentId);
    const QPointF carried =
        parent ? parent->params.value(QStringLiteral("position")).toPointF() : QPointF();

    beginEdit();
    // Keep it where it appears: what the group contributed becomes its own.
    object->params.insert(QStringLiteral("position"),
                          object->params.value(QStringLiteral("position")).toPointF() + carried);
    object->parentId = kInvalidObjectId;
    commit();
}

void EditorState::removeObject(ObjectId id)
{
    if (!m_document.findObject(id))
        return;
    beginEdit();

    // Deleting a group deletes what it holds, which is what the scene list
    // shows and so what the user is asking for.
    QVector<ObjectId> doomed{id};
    for (int i = 0; i < doomed.size(); ++i) {
        for (const SceneObject &object : m_document.objects) {
            if (object.parentId == doomed.at(i) && !doomed.contains(object.id))
                doomed.append(object.id);
        }
    }
    for (int i = doomed.size() - 1; i >= 0; --i)
        m_document.removeObject(doomed.at(i));
    if (m_selectedObject == id)
        clearSelection();
    commit();
}

void EditorState::removeClip(ClipId id)
{
    if (!m_document.findClip(id))
        return;
    beginEdit();
    m_document.removeClip(id);
    if (m_selectedClip == id) {
        m_selectedClip = kInvalidClipId;
        Q_EMIT selectionChanged();
    }
    commit();
}

void EditorState::deleteSelection()
{
    if (m_selectedAudio != kInvalidClipId) {
        removeAudio(m_selectedAudio);
        m_selectedAudio = kInvalidClipId;
        Q_EMIT selectionChanged();
        return;
    }

    // A selected clip is the narrower thing, so it goes first.
    if (m_selectedClip != kInvalidClipId)
        removeClip(m_selectedClip);
    else if (m_selectedObject != kInvalidObjectId)
        removeObject(m_selectedObject);
}

void EditorState::setObjectParam(ObjectId id, const QString &key, const QVariant &value)
{
    SceneObject *object = m_document.findObject(id);
    if (!object || object->params.value(key) == value)
        return;
    object->params.insert(key, value);
    commit();
}

void EditorState::setObjectName(ObjectId id, const QString &name)
{
    SceneObject *object = m_document.findObject(id);
    if (!object || object->name == name)
        return;
    beginEdit();
    object->name = m_document.uniqueObjectName(name);
    commit();
}

void EditorState::moveObjectTo(ObjectId id, const QPointF &scenePosition)
{
    setObjectParam(id, QStringLiteral("position"), scenePosition);
}

void EditorState::setClipParam(ClipId id, const QString &key, const QVariant &value)
{
    Clip *clip = m_document.findClip(id);
    if (!clip || clip->params.value(key) == value)
        return;
    clip->params.insert(key, value);
    commit();
}

void EditorState::setClipTiming(ClipId id, double start, double duration, int track)
{
    Clip *clip = m_document.findClip(id);
    if (!clip)
        return;

    const double newStart = qMax(0.0, start);
    const double newDuration = qMax(0.05, duration);
    const int newTrack = qMax(0, track);

    if (qFuzzyCompare(clip->start + 1.0, newStart + 1.0)
        && qFuzzyCompare(clip->duration + 1.0, newDuration + 1.0) && clip->track == newTrack) {
        return;
    }

    clip->start = newStart;
    clip->duration = newDuration;
    clip->track = newTrack;

    while (m_document.timeline.tracks.size() <= newTrack) {
        Track added;
        added.name = QStringLiteral("Track %1").arg(m_document.timeline.tracks.size() + 1);
        m_document.timeline.tracks.append(added);
    }

    commit();
}

void EditorState::setClipObject(ClipId id, ObjectId objectId)
{
    Clip *clip = m_document.findClip(id);
    if (!clip || clip->objectId == objectId || !m_document.findObject(objectId))
        return;

    clip->objectId = objectId;
    commit();
    Q_EMIT selectionChanged();
}

void EditorState::setClipRateFunction(ClipId id, const QString &name)
{
    Clip *clip = m_document.findClip(id);
    if (!clip || clip->rateFunc == name)
        return;
    beginEdit();
    clip->rateFunc = name;
    commit();
}

ClipId EditorState::addAudio(const QString &sourceFile)
{
    const QFileInfo info(sourceFile);
    if (!info.isFile() || m_layout.assetsDir.isEmpty())
        return kInvalidClipId;

    // Copied in, so the project carries its own sound and stays portable.
    QDir().mkpath(m_layout.assetsDir);
    const QString target = QDir(m_layout.assetsDir).filePath(info.fileName());
    if (QFileInfo(target).absoluteFilePath() != info.absoluteFilePath()) {
        QFile::remove(target);
        if (!QFile::copy(sourceFile, target))
            return kInvalidClipId;
    }

    beginEdit();

    AudioClip clip;
    clip.id = m_nextAudioId++;
    clip.asset = info.fileName();
    clip.start = m_playhead;
    m_document.timeline.audio.append(clip);

    commit();
    return clip.id;
}

void EditorState::setAudioTiming(ClipId id, double start)
{
    for (AudioClip &clip : m_document.timeline.audio) {
        if (clip.id != id)
            continue;
        const double clamped = qMax(0.0, start);
        if (qFuzzyCompare(clip.start + 1.0, clamped + 1.0))
            return;
        clip.start = clamped;
        commit();
        return;
    }
}

void EditorState::setAudioGain(ClipId id, double gain)
{
    for (AudioClip &clip : m_document.timeline.audio) {
        if (clip.id != id)
            continue;
        if (qFuzzyCompare(clip.gain + 1.0, gain + 1.0))
            return;
        clip.gain = gain;
        commit();
        return;
    }
}

void EditorState::removeAudio(ClipId id)
{
    const qsizetype removed = m_document.timeline.audio.removeIf(
        [id](const AudioClip &clip) { return clip.id == id; });
    if (removed == 0)
        return;
    beginEdit();
    commit();
}

void EditorState::setCamera(const Camera3D &camera, bool recordUndo)
{
    if (recordUndo)
        beginEdit();
    m_document.camera = camera;
    commit();
}

void EditorState::addTrack()
{
    beginEdit();
    Track track;
    track.name = QStringLiteral("Track %1").arg(m_document.timeline.tracks.size() + 1);
    m_document.timeline.tracks.append(track);
    commit();
}

void EditorState::removeTrack(int track)
{
    if (track < 0 || track >= m_document.timeline.tracks.size())
        return;

    beginEdit();

    // Everything on the track goes with it; everything below moves up, so no
    // clip is left naming a lane that no longer exists.
    m_document.timeline.clips.removeIf([track](const Clip &clip) { return clip.track == track; });
    for (Clip &clip : m_document.timeline.clips) {
        if (clip.track > track)
            --clip.track;
    }
    m_document.timeline.tracks.remove(track);

    if (!m_document.findClip(m_selectedClip)) {
        m_selectedClip = kInvalidClipId;
        Q_EMIT selectionChanged();
    }

    commit();
}

void EditorState::renameTrack(int track, const QString &name)
{
    if (track < 0 || track >= m_document.timeline.tracks.size())
        return;
    if (m_document.timeline.tracks.at(track).name == name || name.trimmed().isEmpty())
        return;

    beginEdit();
    m_document.timeline.tracks[track].name = name.trimmed();
    commit();
}

void EditorState::setTimelineDuration(double seconds)
{
    const double clamped = qMax(1.0, seconds);
    if (qFuzzyCompare(m_document.timeline.duration + 1.0, clamped + 1.0))
        return;
    beginEdit();
    m_document.timeline.duration = clamped;
    commit();
}

} // namespace mn::ui
