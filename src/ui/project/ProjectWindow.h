#pragma once

#include "AppWindow.h"
#include "Document.h"
#include "Project.h"

class QLabel;

namespace mn::ui {

class CodeEditor;

/// The window a project opens into.
///
/// For now it edits the project's Python directly. The canvas and the timeline
/// will join it as panels beside the code, which is why the project's document
/// is loaded and held here rather than in the editor: the editor is one view of
/// a project, not the project itself.
class ProjectWindow : public AppWindow
{
    Q_OBJECT

public:
    explicit ProjectWindow(QWidget *parent = nullptr);

    /// Load the project at `projectFile`. Reports why not, and returns false.
    bool openProject(const QString &projectFile);

    const Document &document() const { return m_document; }
    ProjectLayout layout() const { return m_layout; }
    QString scriptPath() const { return m_scriptPath; }

    CodeEditor *editor() const { return m_editor; }
    bool isModified() const;

    bool save();

Q_SIGNALS:
    /// Emitted as the window closes, so the launcher can come back.
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildMenus();
    void updateTitle();
    void updateStatus(int line, int column);
    void revealProjectFolder();

    /// Offer to save before discarding. False means the caller should stop.
    bool confirmDiscard();

    Document m_document;
    ProjectLayout m_layout;
    QString m_scriptPath;

    CodeEditor *m_editor = nullptr;
    QLabel *m_scriptLabel = nullptr;
    QLabel *m_positionLabel = nullptr;
};

} // namespace mn::ui
