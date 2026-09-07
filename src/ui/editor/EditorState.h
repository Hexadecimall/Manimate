#pragma once

#include "Document.h"
#include "Project.h"

#include <QJsonObject>
#include <QObject>
#include <QPointF>
#include <QVector>

namespace mn::ui {

/// The project as the editor works on it: the document, what is selected, and
/// where the playhead is.
///
/// Every panel reads from this and every edit goes through it, so the canvas,
/// the timeline and the inspector cannot drift apart. Undo keeps whole document
/// snapshots rather than individual commands — a project's JSON is small, and
/// a snapshot cannot be wrong about what it undoes.
class EditorState : public QObject
{
    Q_OBJECT

public:
    explicit EditorState(QObject *parent = nullptr);

    const Document &document() const { return m_document; }
    Document &documentForWriting() { return m_document; }

    void setDocument(Document document, const ProjectLayout &layout);
    ProjectLayout layout() const { return m_layout; }

    ObjectId selectedObject() const { return m_selectedObject; }
    ClipId selectedClip() const { return m_selectedClip; }
    double playhead() const { return m_playhead; }

    bool isModified() const { return m_modified; }
    void setModified(bool modified);

    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    /// Longest of the explicit duration and whatever the clips actually reach.
    double timelineDuration() const;

public Q_SLOTS:
    void selectObject(ObjectId id);
    void selectClip(ClipId id);
    void clearSelection();
    void setPlayhead(double seconds);

    /// Add a mobject from the catalog at the centre of the frame.
    ObjectId addObject(const QString &specId);

    /// Add an animation for `objectId` at the playhead, on a free track.
    ClipId addClip(ObjectId objectId, const QString &animationId);

    void removeObject(ObjectId id);
    void removeClip(ClipId id);
    void deleteSelection();

    void setObjectParam(ObjectId id, const QString &key, const QVariant &value);
    void setObjectName(ObjectId id, const QString &name);
    void moveObjectTo(ObjectId id, const QPointF &scenePosition);

    void setClipParam(ClipId id, const QString &key, const QVariant &value);
    void setClipTiming(ClipId id, double start, double duration, int track);
    void setClipRateFunction(ClipId id, const QString &name);

    void setTimelineDuration(double seconds);

    void undo();
    void redo();

    /// Record the current document so the next edit can be undone.
    void beginEdit();

Q_SIGNALS:
    void documentChanged();
    void selectionChanged();
    void playheadChanged(double seconds);
    void modifiedChanged(bool modified);
    void historyChanged();

private:
    void commit();
    int freeTrackFor(double start, double duration) const;

    Document m_document;
    ProjectLayout m_layout;

    ObjectId m_selectedObject = kInvalidObjectId;
    ClipId m_selectedClip = kInvalidClipId;
    double m_playhead = 0.0;
    bool m_modified = false;

    QVector<QJsonObject> m_undo;
    QVector<QJsonObject> m_redo;
    static constexpr int kMaxHistory = 100;
};

} // namespace mn::ui
