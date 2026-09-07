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
    void addEmptyState(QVBoxLayout *layout);

    EditorState *m_state;
    QScrollArea *m_scroll = nullptr;
};

} // namespace mn::ui
