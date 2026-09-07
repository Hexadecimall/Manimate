#pragma once

#include "AppWindow.h"
#include "Document.h"
#include "Project.h"

class QLabel;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTimer;

namespace mn::ui {

class CanvasView;
class CodeEditor;
class EditorState;
class InspectorPanel;
class TimelineView;

/// The project window: library, viewer, inspector and timeline, with the
/// project's Python behind a second page.
///
/// The layout follows the shape of an editing application rather than a text
/// editor — what you are working on in the middle, what you can add on the
/// left, what it is made of on the right, and when it happens along the bottom.
class ProjectWindow : public AppWindow
{
    Q_OBJECT

public:
    explicit ProjectWindow(QWidget *parent = nullptr);

    /// Load the project at `projectFile`. Reports why not, and returns false.
    bool openProject(const QString &projectFile);

    const Document &document() const;
    ProjectLayout layout() const { return m_layout; }
    QString scriptPath() const { return m_scriptPath; }

    CodeEditor *editor() const { return m_codeEditor; }
    EditorState *state() const { return m_state; }
    bool isModified() const;

    bool save();

    /// Which page is showing.
    enum class Page { Edit, Code };
    Page currentPage() const;
    void showPage(Page page);

Q_SIGNALS:
    /// Emitted as the window closes, so the launcher can come back.
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *buildEditPage();
    QWidget *buildCodePage();
    QWidget *buildTransportBar();
    QWidget *buildPageBar();

    void buildMenus();
    void updateTitle();
    void updateTransport();
    void updateHistoryActions();

    void togglePlayback();
    void stopPlayback();
    void stepFrame(int frames);
    void advancePlayback();

    void revealProjectFolder();
    bool confirmDiscard();
    bool writeScript();

    EditorState *m_state = nullptr;
    ProjectLayout m_layout;
    QString m_scriptPath;

    QStackedWidget *m_pages = nullptr;
    QPushButton *m_editPageButton = nullptr;
    QPushButton *m_codePageButton = nullptr;

    CanvasView *m_canvas = nullptr;
    TimelineView *m_timeline = nullptr;
    InspectorPanel *m_inspector = nullptr;
    CodeEditor *m_codeEditor = nullptr;

    QPushButton *m_playButton = nullptr;
    QLabel *m_timeLabel = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;

    QTimer *m_playbackTimer = nullptr;
    bool m_playing = false;
};

} // namespace mn::ui
