#pragma once

#include "Catalog.h"
#include "Types.h"

#include <QWidget>

class QFormLayout;
class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace mn::ui {

class EditorState;

/// The properties of whatever is selected.
///
/// Every editor here is built from a catalog ParamSpec rather than written by
/// hand, so a class gains an inspector the moment it gains a catalog entry.
class InspectorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit InspectorPanel(EditorState *state, QWidget *parent = nullptr);

private:
    void rebuild();

    /// Build the right editor for `spec` and wire it to `onChanged`.
    QWidget *editorFor(const catalog::ParamSpec &spec, const QVariant &value,
                       const std::function<void(const QVariant &)> &onChanged);

    /// A titled card holding a form. Returns the form to fill in.
    QFormLayout *addGroup(QVBoxLayout *layout, const QString &title, const QString &subtitle = {});

    void addObjectSection(QVBoxLayout *layout, ObjectId id);
    void addClipSection(QVBoxLayout *layout, ClipId id);
    void addSceneSection(QVBoxLayout *layout);
    void addEmptyState(QVBoxLayout *layout, const QString &message);

    /// A scrolling page holding one section, built fresh on every rebuild.
    QScrollArea *makePage();
    void fillPage(QScrollArea *page, int which);

    EditorState *m_state;
    class SegmentedTabs *m_tabs = nullptr;
    QScrollArea *m_objectPage = nullptr;
    QScrollArea *m_clipPage = nullptr;
    QScrollArea *m_scenePage = nullptr;

    /// True while the tabs are being switched in response to a selection, so
    /// the switch is not mistaken for the user choosing a tab.
    bool m_syncing = false;

    /// A rebuild held back because a field of this panel has focus. Runs once
    /// the focus leaves.
    bool m_rebuildPending = false;
};

} // namespace mn::ui
