#pragma once

#include "AppWindow.h"
#include "Document.h"
#include "Project.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSlider;
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
    enum class Page { Edit, Code, Export };
    Page currentPage() const;
    void showPage(Page page);

Q_SIGNALS:
    /// Emitted as the window closes, so the launcher can come back.
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *buildToolbar();
    QWidget *buildViewerHeader();
    QWidget *buildTimelineBar();
    QWidget *buildEditPage();
    QWidget *buildCodePage();
    QWidget *buildExportPage();

    /// Rewrite the code page from the scene, unless it has been hand-edited.
    void syncCodeFromScene(bool force = false);

    void startRender();
    QWidget *buildTransportBar();
    QWidget *buildPageBar();

    void buildMenus();
    void updateTitle();
    void updateTransport();
    void updateViewerInfo();
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
    QPushButton *m_exportPageButton = nullptr;

    /// The code exactly as generated, so a hand edit can be told apart from a
    /// scene change.
    QString m_generatedCode;
    QWidget *m_codeStaleBar = nullptr;

    class RenderJob *m_renderJob = nullptr;
    QPushButton *m_renderButton = nullptr;
    QPlainTextEdit *m_renderLog = nullptr;
    QLabel *m_renderStatus = nullptr;

    CanvasView *m_canvas = nullptr;
    TimelineView *m_timeline = nullptr;
    InspectorPanel *m_inspector = nullptr;
    CodeEditor *m_codeEditor = nullptr;

    QLabel *m_projectLabel = nullptr;
    QLabel *m_viewerInfoLabel = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QPushButton *m_playButton = nullptr;
    QLabel *m_timeLabel = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;

    QTimer *m_playbackTimer = nullptr;
    bool m_playing = false;
};

} // namespace mn::ui
