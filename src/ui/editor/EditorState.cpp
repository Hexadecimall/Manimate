#include "EditorState.h"

#include "Catalog.h"

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
    // Selecting a clip also selects what it animates, so the inspector and the
    // canvas agree about what is being worked on.
    m_selectedObject = clip ? clip->objectId : kInvalidObjectId;
    Q_EMIT selectionChanged();
}

void EditorState::clearSelection()
{
    if (m_selectedObject == kInvalidObjectId && m_selectedClip == kInvalidClipId)
        return;
    m_selectedObject = kInvalidObjectId;
    m_selectedClip = kInvalidClipId;
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

    Clip clip;
    clip.objectId = objectId;
    clip.type = spec->id;
    clip.start = m_playhead;
    clip.duration = spec->defaultDuration;
    clip.params = catalog::defaultParams(*spec);
    clip.track = freeTrackFor(clip.start, clip.duration);

    while (m_document.timeline.tracks.size() <= clip.track) {
        Track track;
        track.name = QStringLiteral("Track %1").arg(m_document.timeline.tracks.size() + 1);
        m_document.timeline.tracks.append(track);
    }

    const ClipId id = m_document.addClip(std::move(clip));

    commit();
    selectClip(id);
    return id;
}

void EditorState::removeObject(ObjectId id)
{
    if (!m_document.findObject(id))
        return;
    beginEdit();
    m_document.removeObject(id);
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

void EditorState::setClipRateFunction(ClipId id, const QString &name)
{
    Clip *clip = m_document.findClip(id);
    if (!clip || clip->rateFunc == name)
        return;
    beginEdit();
    clip->rateFunc = name;
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
