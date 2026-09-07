#include "ProjectWindow.h"

#include "AudioPlayer.h"
#include "CanvasView.h"
#include "CodeEditor.h"
#include "CodeGenerator.h"
#include "CodeParser.h"
#include "EditorState.h"
#include "RenderJob.h"
#include "InspectorPanel.h"
#include "LibraryPanel.h"
#include "RecentProjects.h"
#include "SceneTemplate.h"
#include "Theme.h"
#include "Version.h"
#include "TimelineView.h"
#include "TitleBar.h"

#include <QAction>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace mn::ui {
namespace {

constexpr int kLibraryWidth = 236;
constexpr int kInspectorWidth = 288;
constexpr int kPageBarHeight = 44;

QPushButton *transportButton(const QString &text, const QString &tip)
{
    auto *button = new QPushButton(text);
    button->setProperty("role", "quiet");
    button->setToolTip(tip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(28);
    button->setMinimumWidth(34);
    return button;
}

QString formatTimecode(double seconds, int fps)
{
    const int totalFrames = int(std::round(seconds * fps));
    const int frames = totalFrames % qMax(1, fps);
    const int wholeSeconds = totalFrames / qMax(1, fps);
    return QStringLiteral("%1:%2.%3")
        .arg(wholeSeconds / 60, 2, 10, QLatin1Char('0'))
        .arg(wholeSeconds % 60, 2, 10, QLatin1Char('0'))
        .arg(frames, 2, 10, QLatin1Char('0'));
}

} // namespace

ProjectWindow::ProjectWindow(QWidget *parent)
    : AppWindow(parent)
    , m_state(new EditorState(this))
{
    resize(1420, 900);
    setMinimumSize(920, 600);

    m_playbackTimer = new QTimer(this);
    // The default coarse timer can slip by several milliseconds a tick, which
    // at 60fps is a visible stutter.
    m_playbackTimer->setTimerType(Qt::PreciseTimer);
    connect(m_playbackTimer, &QTimer::timeout, this, &ProjectWindow::advancePlayback);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_audio = new AudioPlayer(this);
    m_renderJob = new RenderJob(this);

    m_pages = new QStackedWidget;
    m_pages->addWidget(buildEditPage());
    m_pages->addWidget(buildCodePage());
    m_pages->addWidget(buildExportPage());
    layout->addWidget(m_pages, 1);
    layout->addWidget(buildPageBar());

    setContent(content);
    buildMenus();
    buildTitleBarActions();

    connect(m_state, &EditorState::modifiedChanged, this, [this] { updateTitle(); });
    connect(m_state, &EditorState::playheadChanged, this, [this](double seconds) {
        updateTransport();
        m_audio->seek(m_state->document().timeline, seconds);
    });
    connect(m_state, &EditorState::documentChanged, this, [this] {
        updateTransport();
        updateViewerInfo();
        syncCodeFromScene();
    });
    connect(m_state, &EditorState::historyChanged, this, &ProjectWindow::updateHistoryActions);

    // The window remembers where it was and how it was arranged. Restored
    // after everything exists, so there is something to restore onto.
    rememberGeometryAs(QStringLiteral("project"));

    QSettings settings;
    const QByteArray columns = settings.value(QStringLiteral("editor/columns")).toByteArray();
    if (!columns.isEmpty())
        m_columns->restoreState(columns);
    const QByteArray rows = settings.value(QStringLiteral("editor/rows")).toByteArray();
    if (!rows.isEmpty())
        m_rows->restoreState(rows);

    const double zoom = settings.value(QStringLiteral("editor/timelineZoom"), 90.0).toDouble();
    m_timeline->setScale(zoom);
    if (m_zoomSlider)
        m_zoomSlider->setValue(int(m_timeline->scale()));

    const bool guides = settings.value(QStringLiteral("editor/guides"), true).toBool();
    m_guidesButton->setChecked(guides);
    m_canvas->setGuidesVisible(guides);

    const int page = settings.value(QStringLiteral("editor/page"), 0).toInt();
    showPage(page == 1 ? Page::Code : page == 2 ? Page::Export : Page::Edit);

    updateTitle();
    updateTransport();
}

void ProjectWindow::saveWindowState()
{
    AppWindow::saveWindowState();

    QSettings settings;
    if (m_columns)
        settings.setValue(QStringLiteral("editor/columns"), m_columns->saveState());
    if (m_rows)
        settings.setValue(QStringLiteral("editor/rows"), m_rows->saveState());
    if (m_timeline)
        settings.setValue(QStringLiteral("editor/timelineZoom"), m_timeline->scale());
    if (m_guidesButton)
        settings.setValue(QStringLiteral("editor/guides"), m_guidesButton->isChecked());
    settings.setValue(QStringLiteral("editor/page"), m_pages ? m_pages->currentIndex() : 0);
}

void ProjectWindow::buildTitleBarActions()
{
    auto *undoButton = new QPushButton(tr("Undo"));
    auto *redoButton = new QPushButton(tr("Redo"));
    for (QPushButton *button : {undoButton, redoButton}) {
        button->setProperty("role", "quiet");
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedHeight(24);
        button->setEnabled(false);
    }

    connect(undoButton, &QPushButton::clicked, m_state, &EditorState::undo);
    connect(redoButton, &QPushButton::clicked, m_state, &EditorState::redo);
    connect(m_state, &EditorState::historyChanged, this, [this, undoButton, redoButton] {
        undoButton->setEnabled(m_state->canUndo());
        redoButton->setEnabled(m_state->canRedo());
    });

    titleBar()->addTrailingWidget(undoButton);
    titleBar()->addTrailingWidget(redoButton);
}

QWidget *ProjectWindow::buildViewerHeader()
{
    const theme::Palette &p = theme::palette();

    auto *bar = new QWidget;
    bar->setObjectName(QStringLiteral("viewerHeader"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setFixedHeight(30);
    bar->setStyleSheet(
        QStringLiteral("QWidget#viewerHeader { background: %1; border-bottom: 1px solid %2; }")
            .arg(p.surfaceRaised.name(), p.border.name()));

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(14, 0, 14, 0);

    auto *title = new QLabel(tr("VIEWER"));
    title->setProperty("role", "panelTitle");
    layout->addWidget(title);
    layout->addStretch(1);

    m_viewerInfoLabel = new QLabel;
    m_viewerInfoLabel->setProperty("role", "subtitle");
    layout->addWidget(m_viewerInfoLabel);

    return bar;
}

QWidget *ProjectWindow::buildTimelineBar()
{
    const theme::Palette &p = theme::palette();

    auto *bar = new QWidget;
    bar->setObjectName(QStringLiteral("timelineBar"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setFixedHeight(30);
    bar->setStyleSheet(QStringLiteral("QWidget#timelineBar { background: %1;"
                                      " border-top: 1px solid %2;"
                                      " border-bottom: 1px solid %2; }")
                           .arg(p.surfaceRaised.name(), p.border.name()));

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(14, 0, 14, 0);
    layout->setSpacing(10);

    auto *title = new QLabel(tr("TIMELINE"));
    title->setProperty("role", "panelTitle");
    layout->addWidget(title);
    layout->addStretch(1);

    auto *addAudio = new QPushButton(tr("Add Sound…"));
    addAudio->setProperty("role", "quiet");
    addAudio->setCursor(Qt::PointingHandCursor);
    addAudio->setFixedHeight(24);
    connect(addAudio, &QPushButton::clicked, this, &ProjectWindow::addSound);
    layout->addWidget(addAudio);
    layout->addSpacing(8);

    auto *zoomLabel = new QLabel(tr("Zoom"));
    zoomLabel->setProperty("role", "subtitle");
    layout->addWidget(zoomLabel);

    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setFixedWidth(130);
    m_zoomSlider->setRange(12, 400);
    m_zoomSlider->setValue(90);
    connect(m_zoomSlider, &QSlider::valueChanged, this,
            [this](int value) { m_timeline->setScale(value); });
    layout->addWidget(m_zoomSlider);

    return bar;
}

QWidget *ProjectWindow::buildEditPage()
{
    const theme::Palette &p = theme::palette();

    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Viewer with its transport, between the library and the inspector.
    auto *centre = new QWidget;
    auto *centreLayout = new QVBoxLayout(centre);
    centreLayout->setContentsMargins(0, 0, 0, 0);
    centreLayout->setSpacing(0);

    centreLayout->addWidget(buildViewerHeader());
    m_canvas = new CanvasView(m_state);
    centreLayout->addWidget(m_canvas, 1);
    centreLayout->addWidget(buildTransportBar());

    auto *leftColumn = new LibraryPanel(m_state);
    leftColumn->setMinimumWidth(190);
    m_inspector = new InspectorPanel(m_state);
    m_inspector->setMinimumWidth(220);

    m_columns = new QSplitter(Qt::Horizontal);
    QSplitter *upper = m_columns;
    upper->setHandleWidth(1);
    upper->setChildrenCollapsible(false);
    upper->addWidget(leftColumn);
    upper->addWidget(centre);
    upper->addWidget(m_inspector);
    upper->setStretchFactor(0, 0);
    upper->setStretchFactor(1, 1);
    upper->setStretchFactor(2, 0);
    upper->setSizes({kLibraryWidth, 900, kInspectorWidth});

    m_timeline = new TimelineView(m_state);

    auto *timelineColumn = new QWidget;
    auto *timelineLayout = new QVBoxLayout(timelineColumn);
    timelineLayout->setContentsMargins(0, 0, 0, 0);
    timelineLayout->setSpacing(0);
    auto *timelineScroll = new QScrollArea;
    timelineScroll->setWidget(m_timeline);
    timelineScroll->setWidgetResizable(true);
    timelineScroll->setFrameShape(QFrame::NoFrame);
    timelineScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    timelineScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    timelineLayout->addWidget(buildTimelineBar());
    timelineLayout->addWidget(timelineScroll, 1);

    m_rows = new QSplitter(Qt::Vertical);
    QSplitter *split = m_rows;
    split->setHandleWidth(1);
    split->setChildrenCollapsible(false);
    split->addWidget(upper);
    split->addWidget(timelineColumn);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    split->setSizes({560, 320});
    split->setStyleSheet(QStringLiteral("QSplitter::handle { background: %1; }").arg(p.border.name()));
    upper->setStyleSheet(QStringLiteral("QSplitter::handle { background: %1; }").arg(p.border.name()));

    layout->addWidget(split, 1);
    return page;
}

QWidget *ProjectWindow::buildTransportBar()
{
    const theme::Palette &p = theme::palette();

    auto *bar = new QWidget;
    bar->setObjectName(QStringLiteral("transportBar"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setFixedHeight(40);
    bar->setStyleSheet(
        QStringLiteral("QWidget#transportBar { background: %1; border-top: 1px solid %2; }")
            .arg(p.window.name(), p.border.name()));

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(14, 0, 14, 0);
    layout->setSpacing(4);

    auto *toStart = transportButton(QStringLiteral("|◀"), tr("Go to start"));
    auto *back = transportButton(QStringLiteral("◀|"), tr("Previous frame"));
    m_playButton = transportButton(QStringLiteral("▶"), tr("Play"));
    auto *forward = transportButton(QStringLiteral("|▶"), tr("Next frame"));
    auto *toEnd = transportButton(QStringLiteral("▶|"), tr("Go to end"));

    connect(toStart, &QPushButton::clicked, this, [this] {
        stopPlayback();
        m_state->setPlayhead(0);
    });
    connect(back, &QPushButton::clicked, this, [this] { stepFrame(-1); });
    connect(m_playButton, &QPushButton::clicked, this, &ProjectWindow::togglePlayback);
    connect(forward, &QPushButton::clicked, this, [this] { stepFrame(1); });
    connect(toEnd, &QPushButton::clicked, this, [this] {
        stopPlayback();
        m_state->setPlayhead(m_state->timelineDuration());
    });

    layout->addWidget(toStart);
    layout->addWidget(back);
    layout->addWidget(m_playButton);
    layout->addWidget(forward);
    layout->addWidget(toEnd);
    layout->addSpacing(14);

    m_timeLabel = new QLabel;
    QFont mono = theme::font(1);
    mono.setStyleHint(QFont::Monospace);
    mono.setFamily(QStringLiteral("Menlo"));
    mono.setPixelSize(12);
    m_timeLabel->setFont(mono);
    m_timeLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(p.text.name()));
    layout->addWidget(m_timeLabel);

    layout->addStretch(1);

    m_guidesButton = new QPushButton(tr("Guides"));
    m_guidesButton->setProperty("role", "quiet");
    m_guidesButton->setCheckable(true);
    m_guidesButton->setChecked(true);
    m_guidesButton->setCursor(Qt::PointingHandCursor);
    connect(m_guidesButton, &QPushButton::toggled, this,
            [this](bool on) { m_canvas->setGuidesVisible(on); });
    layout->addWidget(m_guidesButton);

    return bar;
}

QWidget *ProjectWindow::buildCodePage()
{
    const theme::Palette &p = theme::palette();

    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    const theme::Palette &warn = theme::palette();
    m_codeStaleBar = new QWidget;
    m_codeStaleBar->setFixedHeight(34);
    m_codeStaleBar->setObjectName(QStringLiteral("codeStaleBar"));
    m_codeStaleBar->setAttribute(Qt::WA_StyledBackground, true);
    m_codeStaleBar->setStyleSheet(
        QStringLiteral("QWidget#codeStaleBar { background: %1; border-bottom: 1px solid %2; }")
            .arg(theme::mix(warn.surfaceRaised, warn.accent, 0.22).name(), warn.border.name()));

    auto *staleLayout = new QHBoxLayout(m_codeStaleBar);
    staleLayout->setContentsMargins(14, 0, 10, 0);
    auto *staleText = new QLabel(tr("This code and the scene have both changed."));
    staleText->setProperty("role", "subtitle");
    staleLayout->addWidget(staleText);
    staleLayout->addStretch(1);

    auto *toScene = new QPushButton(tr("Apply to scene"));
    toScene->setCursor(Qt::PointingHandCursor);
    toScene->setFixedHeight(24);
    connect(toScene, &QPushButton::clicked, this, &ProjectWindow::applyCodeToScene);
    staleLayout->addWidget(toScene);

    auto *regenerate = new QPushButton(tr("Regenerate from scene"));
    regenerate->setCursor(Qt::PointingHandCursor);
    regenerate->setFixedHeight(24);
    connect(regenerate, &QPushButton::clicked, this, [this] { syncCodeFromScene(true); });
    staleLayout->addWidget(regenerate);

    m_codeStaleBar->setVisible(false);
    layout->addWidget(m_codeStaleBar);

    m_codeEditor = new CodeEditor;
    layout->addWidget(m_codeEditor, 1);

    auto *status = new QWidget;
    status->setObjectName(QStringLiteral("codeStatus"));
    status->setAttribute(Qt::WA_StyledBackground, true);
    status->setFixedHeight(26);
    status->setStyleSheet(
        QStringLiteral("QWidget#codeStatus { background: %1; border-top: 1px solid %2; }")
            .arg(p.window.name(), p.border.name()));

    auto *scriptLabel = new QLabel;
    scriptLabel->setProperty("role", "subtitle");
    auto *positionLabel = new QLabel;
    positionLabel->setProperty("role", "subtitle");

    auto *statusLayout = new QHBoxLayout(status);
    statusLayout->setContentsMargins(14, 0, 14, 0);
    statusLayout->addWidget(scriptLabel, 1);
    statusLayout->addWidget(positionLabel);
    layout->addWidget(status);

    connect(m_codeEditor, &CodeEditor::cursorPositionDescribed, this,
            [this, positionLabel](int line, int column) {
                positionLabel->setText(tr("Line %1, Column %2  ·  %3 lines")
                                           .arg(line)
                                           .arg(column)
                                           .arg(m_codeEditor->lineCount()));
            });
    connect(m_codeEditor->document(), &QTextDocument::modificationChanged, this, [this] {
        updateTitle();
        // Editing the code offers a way to push it back into the scene.
        if (m_codeStaleBar)
            m_codeStaleBar->setVisible(m_codeEditor->document()->isModified());
    });
    connect(this, &ProjectWindow::closed, scriptLabel, [] {});

    // Kept up to date whenever a project is opened.
    scriptLabel->setObjectName(QStringLiteral("scriptLabel"));
    return page;
}

QWidget *ProjectWindow::buildExportPage()
{
    const theme::Palette &p = theme::palette();

    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *header = new QWidget;
    header->setProperty("role", "panelHeader");
    header->setFixedHeight(32);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 0, 14, 0);
    auto *headerTitle = new QLabel(tr("EXPORT"));
    headerTitle->setProperty("role", "panelTitle");
    headerLayout->addWidget(headerTitle);
    headerLayout->addStretch(1);

    m_renderStatus = new QLabel;
    m_renderStatus->setProperty("role", "subtitle");
    headerLayout->addWidget(m_renderStatus);
    layout->addWidget(header);

    auto *body = new QWidget;
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(22, 20, 22, 20);
    bodyLayout->setSpacing(14);

    auto *explain = new QLabel(
        tr("Renders this project's Python with Manim and puts the video in the "
           "project's output folder. The render runs alongside the editor, so "
           "you can keep working while it goes."));

    auto *provenance = new QLabel;
    provenance->setProperty("role", "subtitle");
    provenance->setWordWrap(true);
    {
        QString manim;
        const bool have = RenderJob::manimAvailable(&manim);
        const bool bundled = RenderJob::usingBundledPython();
        if (have && bundled)
            provenance->setText(tr("Using the Manim %1 shipped with Manimate.").arg(manim));
        else if (have)
            provenance->setText(tr("Using Manim %1 from this machine's Python.").arg(manim));
        else
            provenance->setText(tr("No Manim was found. This build ships without one; "
                                   "install it with:  python3 -m pip install manim"));
    }
    explain->setProperty("role", "subtitle");
    explain->setWordWrap(true);
    bodyLayout->addWidget(explain);
    bodyLayout->addWidget(provenance);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(10);

    m_renderButton = new QPushButton(tr("Render Video"));
    m_renderButton->setProperty("role", "primary");
    m_renderButton->setCursor(Qt::PointingHandCursor);
    m_renderButton->setMinimumHeight(34);
    connect(m_renderButton, &QPushButton::clicked, this, &ProjectWindow::startRender);

    auto *openFolder = new QPushButton(tr("Open Output Folder"));
    openFolder->setCursor(Qt::PointingHandCursor);
    openFolder->setMinimumHeight(34);
    connect(openFolder, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_layout.outputDir));
    });

    buttons->addWidget(m_renderButton);
    buttons->addWidget(openFolder);
    buttons->addStretch(1);
    bodyLayout->addLayout(buttons);

    m_renderLog = new QPlainTextEdit;
    m_renderLog->setReadOnly(true);
    m_renderLog->setFrameShape(QFrame::NoFrame);
    QFont mono = theme::font(1);
    mono.setStyleHint(QFont::Monospace);
    mono.setFamily(QStringLiteral("Menlo"));
    mono.setPixelSize(11);
    m_renderLog->setFont(mono);
    m_renderLog->setStyleSheet(QStringLiteral("QPlainTextEdit { background: %1; color: %2;"
                                              " border: 1px solid %3; border-radius: 6px; }")
                                   .arg(p.window.name(), p.textMuted.name(), p.border.name()));
    bodyLayout->addWidget(m_renderLog, 1);

    layout->addWidget(body, 1);

    connect(m_renderJob, &RenderJob::started, this, [this](const QString &command) {
        m_renderLog->clear();
        m_renderLog->appendPlainText(command);
        m_renderLog->appendPlainText(QString());
        m_renderButton->setText(tr("Cancel Render"));
        m_renderStatus->setText(tr("Rendering…"));
    });
    connect(m_renderJob, &RenderJob::output, this,
            [this](const QString &line) { m_renderLog->appendPlainText(line); });
    connect(m_renderJob, &RenderJob::finished, this, [this](bool ok, const QString &message) {
        m_renderLog->appendPlainText(QString());
        m_renderLog->appendPlainText(message);
        m_renderButton->setText(tr("Render Video"));
        m_renderStatus->setText(ok ? tr("Done") : tr("Failed"));
    });

    return page;
}

void ProjectWindow::startRender()
{
    if (m_renderJob->isRunning()) {
        m_renderJob->cancel();
        return;
    }

    // Whatever is on screen is what gets rendered, so save it first.
    if (!save())
        return;

    QString version;
    if (!RenderJob::manimAvailable(&version)) {
        m_renderLog->clear();
        m_renderLog->appendPlainText(
            tr("Manim is not importable by the Python on this system.\n"
               "Install it with:  python3 -m pip install manim"));
        m_renderStatus->setText(tr("Manim not found"));
        return;
    }

    // A scene using formulae needs LaTeX, which cannot be bundled. Say which
    // objects need it, rather than letting Manim raise FileNotFoundError.
    const QStringList needLatex = RenderJob::objectsNeedingLatex(m_state->document());
    if (!needLatex.isEmpty() && !RenderJob::latexAvailable()) {
        m_renderLog->clear();
        m_renderLog->appendPlainText(
            tr("These objects are typeset through LaTeX, which is not installed:\n"
               "    %1\n\n"
               "Manim cannot render them without it. Either remove them, or install a\n"
               "TeX distribution (MacTeX, TeX Live or MiKTeX) and render again.\n"
               "Everything else in the scene would render fine.")
                .arg(needLatex.join(QStringLiteral(", "))));
        m_renderStatus->setText(tr("LaTeX not found"));
        return;
    }

    m_renderStatus->setText(tr("Manim %1").arg(version));
    m_renderJob->start(m_layout, m_scriptPath, m_state->document().sceneClassName,
                       m_state->document().render);
}

void ProjectWindow::applyCodeToScene()
{
    Document rebuilt = m_state->document();
    const codeparser::Result outcome = codeparser::parse(m_codeEditor->toPlainText(), &rebuilt);

    if (!outcome.ok()) {
        QMessageBox::warning(this, tr("Cannot read that"), outcome.error);
        return;
    }

    if (!outcome.complete) {
        // Say exactly which lines will not survive, before anything is lost.
        QMessageBox box(QMessageBox::Warning, tr("Some code cannot be read"),
                        tr("%n line(s) are outside what the editor can represent.", nullptr,
                           int(outcome.unrecognised.size())),
                        QMessageBox::Ok | QMessageBox::Cancel, this);
        box.setInformativeText(
            tr("Applying this will keep everything else and drop those lines. They will "
               "still be in the file until the scene is written out again."));
        box.setDetailedText(outcome.unrecognised.join(QStringLiteral("\n")));
        box.setDefaultButton(QMessageBox::Cancel);
        if (box.exec() != QMessageBox::Ok)
            return;
    }

    m_state->beginEdit();
    m_state->setDocument(std::move(rebuilt), m_layout);
    m_state->setModified(true);

    // The code now matches the scene, so the two are back in step.
    m_generatedCode = codegen::generate(m_state->document());
    m_codeEditor->setPlainText(m_generatedCode);
    if (m_codeStaleBar)
        m_codeStaleBar->setVisible(false);

    showPage(Page::Edit);
}

void ProjectWindow::syncCodeFromScene(bool force)
{
    const QString generated = codegen::generate(m_state->document());
    if (generated == m_generatedCode && !force)
        return;

    const bool handEdited = !m_generatedCode.isEmpty()
                            && m_codeEditor->toPlainText() != m_generatedCode;

    if (handEdited && !force) {
        // The scene moved on, but so did the code, by hand. Say so rather than
        // silently throwing away whichever one loses.
        if (m_codeStaleBar)
            m_codeStaleBar->setVisible(true);
        return;
    }

    const int scrollPosition = m_codeEditor->verticalScrollBar()->value();
    m_codeEditor->setPlainText(generated);
    m_codeEditor->verticalScrollBar()->setValue(scrollPosition);
    m_generatedCode = generated;

    if (m_codeStaleBar)
        m_codeStaleBar->setVisible(false);
}

QWidget *ProjectWindow::buildPageBar()
{
    const theme::Palette &p = theme::palette();

    auto *bar = new QWidget;
    bar->setObjectName(QStringLiteral("pageBar"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setFixedHeight(kPageBarHeight);
    bar->setStyleSheet(QStringLiteral("QWidget#pageBar {"
                                      "  background: %1;"
                                      "  border-top: 1px solid %2;"
                                      "  border-bottom-left-radius: %3px;"
                                      "  border-bottom-right-radius: %3px;"
                                      "}")
                           .arg(p.surface.name(), p.border.name())
                           .arg(AppWindow::kInnerCornerRadius));

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(14, 6, 14, 6);
    layout->setSpacing(6);

    auto makePageButton = [&p](const QString &text) {
        auto *button = new QPushButton(text);
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumWidth(96);
        button->setStyleSheet(
            QStringLiteral("QPushButton {"
                           "  background: transparent;"
                           "  border: none;"
                           "  border-radius: 6px;"
                           "  color: %1;"
                           "  padding: 6px 18px;"
                           "  font-size: 12px;"
                           "}"
                           "QPushButton:hover { background: %2; color: %3; }"
                           "QPushButton:checked { background: %4; color: %5; font-weight: 600; }")
                .arg(p.textMuted.name(), p.surfaceHover.name(), p.text.name(),
                     p.surfaceActive.name(), p.text.name()));
        return button;
    };

    m_editPageButton = makePageButton(tr("Edit"));
    m_codePageButton = makePageButton(tr("Code"));
    m_exportPageButton = makePageButton(tr("Export"));

    connect(m_editPageButton, &QPushButton::clicked, this, [this] { showPage(Page::Edit); });
    connect(m_codePageButton, &QPushButton::clicked, this, [this] { showPage(Page::Code); });
    connect(m_exportPageButton, &QPushButton::clicked, this, [this] { showPage(Page::Export); });

    layout->addStretch(1);
    layout->addWidget(m_editPageButton);
    layout->addWidget(m_codePageButton);
    layout->addWidget(m_exportPageButton);
    layout->addStretch(1);

    return bar;
}

ProjectWindow::Page ProjectWindow::currentPage() const
{
    switch (m_pages->currentIndex()) {
    case 0:
        return Page::Edit;
    case 1:
        return Page::Code;
    default:
        return Page::Export;
    }
}

void ProjectWindow::showPage(Page page)
{
    m_pages->setCurrentIndex(page == Page::Edit ? 0 : page == Page::Code ? 1 : 2);
    m_editPageButton->setChecked(page == Page::Edit);
    m_codePageButton->setChecked(page == Page::Code);
    m_exportPageButton->setChecked(page == Page::Export);

    if (page != Page::Edit)
        stopPlayback();
}

void ProjectWindow::buildMenus()
{
    // Everything here also has to work without the menu, so each action is a
    // thin wrapper over something the interface already does.
    auto *fileMenu = new QMenu(tr("File"), this);
    QAction *saveAction = fileMenu->addAction(tr("Save"), QKeySequence::Save, this, [this] { save(); });
    fileMenu->addSeparator();
    QAction *renderAction = fileMenu->addAction(tr("Render Video…"), QKeySequence(tr("Ctrl+R")), this,
                                                [this] {
                                                    showPage(Page::Export);
                                                    startRender();
                                                });
    QAction *revealAction = fileMenu->addAction(tr("Show Project Folder"), this,
                                                &ProjectWindow::revealProjectFolder);
    QAction *outputAction = fileMenu->addAction(tr("Show Rendered Video"), this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_layout.outputDir));
    });
    fileMenu->addSeparator();
    QAction *closeAction = fileMenu->addAction(tr("Close Project"), QKeySequence::Close, this,
                                               [this] { close(); });

    auto *editMenu = new QMenu(tr("Edit"), this);
    m_undoAction = editMenu->addAction(tr("Undo"), QKeySequence::Undo, m_state, &EditorState::undo);
    m_redoAction = editMenu->addAction(tr("Redo"), QKeySequence::Redo, m_state, &EditorState::redo);
    editMenu->addSeparator();
    QAction *deleteAction = editMenu->addAction(tr("Delete"), QKeySequence::Delete, m_state,
                                                &EditorState::deleteSelection);
    QAction *ungroupAction = editMenu->addAction(tr("Ungroup"), QKeySequence(tr("Ctrl+Shift+G")),
                                                 this, [this] {
                                                     m_state->ungroupObject(m_state->selectedObject());
                                                 });

    auto *insertMenu = new QMenu(tr("Insert"), this);
    QAction *trackAction = insertMenu->addAction(tr("Add Track"), QKeySequence(tr("Ctrl+T")), this,
                                                 [this] { m_state->addTrack(); });
    QAction *soundAction = insertMenu->addAction(tr("Add Sound…"), this, [this] { addSound(); });
    insertMenu->addSeparator();
    QAction *blockAction = insertMenu->addAction(
        tr("Python Block"), QKeySequence(tr("Ctrl+Shift+P")), this, [this] {
            const ObjectId selected = m_state->selectedObject();
            if (selected == kInvalidObjectId) {
                QMessageBox::information(this, tr("Nothing selected"),
                                         tr("A Python block attaches to an object, so it knows "
                                            "what TARGET refers to. Select one first."));
                return;
            }
            m_state->addClip(selected, QStringLiteral("manim.Code"));
        });

    auto *viewMenu = new QMenu(tr("View"), this);
    QAction *editPage = viewMenu->addAction(tr("Edit"), QKeySequence(tr("Ctrl+1")), this,
                                            [this] { showPage(Page::Edit); });
    QAction *codePage = viewMenu->addAction(tr("Code"), QKeySequence(tr("Ctrl+2")), this,
                                            [this] { showPage(Page::Code); });
    QAction *exportPage = viewMenu->addAction(tr("Export"), QKeySequence(tr("Ctrl+3")), this,
                                              [this] { showPage(Page::Export); });
    viewMenu->addSeparator();
    QAction *playAction = viewMenu->addAction(tr("Play / Pause"), QKeySequence(Qt::Key_Space), this,
                                              &ProjectWindow::togglePlayback);
    QAction *startAction = viewMenu->addAction(tr("Go to Start"), QKeySequence(Qt::Key_Home), this,
                                               [this] {
                                                   stopPlayback();
                                                   m_state->setPlayhead(0);
                                               });
    QAction *endAction = viewMenu->addAction(tr("Go to End"), QKeySequence(Qt::Key_End), this,
                                             [this] {
                                                 stopPlayback();
                                                 m_state->setPlayhead(m_state->timelineDuration());
                                             });
    viewMenu->addSeparator();
    QAction *zoomIn = viewMenu->addAction(tr("Zoom In"), QKeySequence::ZoomIn, this, [this] {
        m_timeline->setScale(m_timeline->scale() * 1.25);
        if (m_zoomSlider)
            m_zoomSlider->setValue(int(m_timeline->scale()));
    });
    QAction *zoomOut = viewMenu->addAction(tr("Zoom Out"), QKeySequence::ZoomOut, this, [this] {
        m_timeline->setScale(m_timeline->scale() / 1.25);
        if (m_zoomSlider)
            m_zoomSlider->setValue(int(m_timeline->scale()));
    });

    auto *helpMenu = new QMenu(tr("Help"), this);
    QAction *shortcutsAction = helpMenu->addAction(tr("Keyboard Shortcuts"), this,
                                                   &ProjectWindow::showShortcuts);
    helpMenu->addSeparator();
    QAction *manimDocs = helpMenu->addAction(tr("Manim Documentation"), this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://docs.manim.community")));
    });
    QAction *manimExamples = helpMenu->addAction(tr("Manim Examples"), this, [] {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://docs.manim.community/en/stable/examples.html")));
    });
    helpMenu->addSeparator();
    QAction *reportAction = helpMenu->addAction(tr("Report an Issue"), this, [] {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://github.com/Hexadecimall/Manimate/issues")));
    });
    QAction *aboutAction = helpMenu->addAction(tr("About Manimate"), this, [this] {
        QMessageBox box(QMessageBox::NoIcon, tr("About Manimate"),
                        tr("Manimate %1").arg(version::string()), QMessageBox::Ok, this);
        QString manim;
        box.setInformativeText(RenderJob::manimAvailable(&manim)
                                   ? tr("A visual editor for Manim.\n\nUsing Manim %1.").arg(manim)
                                   : tr("A visual editor for Manim.\n\nManim was not found."));
        box.exec();
    });

    for (QAction *action : {saveAction, renderAction, revealAction, outputAction, closeAction,
                            m_undoAction, m_redoAction, deleteAction, ungroupAction, trackAction,
                            soundAction, blockAction, editPage, codePage, exportPage, playAction,
                            startAction, endAction, zoomIn, zoomOut, shortcutsAction, manimDocs,
                            manimExamples, reportAction}) {
        action->setMenuRole(QAction::NoRole);
    }
    // About belongs in the application menu on macOS, where it is looked for.
    aboutAction->setMenuRole(QAction::AboutRole);

    setMenus({fileMenu, editMenu, insertMenu, viewMenu, helpMenu});
    updateHistoryActions();
}

void ProjectWindow::showShortcuts()
{
    QMessageBox box(QMessageBox::NoIcon, tr("Keyboard Shortcuts"), tr("Keyboard Shortcuts"),
                    QMessageBox::Ok, this);
    box.setInformativeText(tr(
        "Canvas\n"
        "    Drag                 move the selected object\n"
        "    Shift-drag           snap to a quarter unit\n"
        "    Drag a corner        resize\n"
        "    Arrow keys           nudge, with shift for further\n"
        "    Backspace            delete\n"
        "\n"
        "Timeline\n"
        "    Drag a clip          retime it\n"
        "    Drag a clip edge     trim it\n"
        "    Alt-drag             ignore snapping\n"
        "    Drag the ruler       scrub\n"
        "    Shift-scroll         scroll along\n"
        "    Ctrl-scroll          zoom about the pointer\n"
        "    Right-click          add, rename or delete a track\n"
        "\n"
        "Playback\n"
        "    Space                play or pause\n"
        "    Home / End           go to the start or the end"));
    box.exec();
}

void ProjectWindow::updateHistoryActions()
{
    if (m_undoAction)
        m_undoAction->setEnabled(m_state->canUndo());
    if (m_redoAction)
        m_redoAction->setEnabled(m_state->canRedo());
}

const Document &ProjectWindow::document() const
{
    return m_state->document();
}

bool ProjectWindow::isModified() const
{
    return m_state->isModified() || m_codeEditor->document()->isModified();
}

bool ProjectWindow::openProject(const QString &projectFile)
{
    const auto resolved = project::resolve(projectFile);
    if (!resolved) {
        QMessageBox::warning(this, tr("Cannot open that"),
                             tr("%1 is not a Manimate project.")
                                 .arg(QDir::toNativeSeparators(projectFile)));
        return false;
    }
    m_layout = *resolved;

    QString error;
    Document document;
    if (!project::ensureDirectories(m_layout, &error)
        || !Document::load(m_layout.projectFile, &document, &error)) {
        QMessageBox::warning(this, tr("Cannot open that"), error);
        return false;
    }

    m_scriptPath = scene_template::ensureScript(m_layout, document, &error);
    if (m_scriptPath.isEmpty()) {
        QMessageBox::warning(this, tr("Cannot open that"), error);
        return false;
    }

    QFile file(m_scriptPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Cannot open that"), file.errorString());
        return false;
    }
    QTextStream stream(&file);
    m_codeEditor->setPlainText(stream.readAll());
    m_codeEditor->document()->setModified(false);

    if (auto *label = m_pages->widget(1)->findChild<QLabel *>(QStringLiteral("scriptLabel"))) {
        label->setText(QDir::toNativeSeparators(QDir(m_layout.root).relativeFilePath(m_scriptPath)));
    }

    m_audio->setLayout(m_layout);
    m_state->setDocument(std::move(document), m_layout);
    m_generatedCode.clear();
    syncCodeFromScene(m_codeEditor->toPlainText().trimmed().isEmpty());

    RecentProjects::touch(m_layout.projectFile, m_state->document().metadata.name,
                          m_state->document().metadata.description);

    updateTitle();
    updateTransport();
    updateViewerInfo();
    if (m_zoomSlider && m_timeline)
        m_zoomSlider->setValue(int(m_timeline->scale()));
    m_canvas->setFocus();
    return true;
}

bool ProjectWindow::writeScript()
{
    if (m_scriptPath.isEmpty())
        return true;

    QSaveFile file(m_scriptPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Cannot save that"), file.errorString());
        return false;
    }

    QTextStream stream(&file);
    stream << m_codeEditor->toPlainText();
    stream.flush();
    if (!file.commit()) {
        QMessageBox::warning(this, tr("Cannot save that"), file.errorString());
        return false;
    }

    m_codeEditor->document()->setModified(false);
    return true;
}

bool ProjectWindow::save()
{
    if (!writeScript())
        return false;

    QString error;
    if (!m_state->documentForWriting().save(m_layout.projectFile, &error)) {
        QMessageBox::warning(this, tr("Cannot save that"), error);
        return false;
    }

    m_state->setModified(false);
    updateTitle();
    return true;
}

void ProjectWindow::updateTitle()
{
    const QString name = m_state->document().metadata.name.isEmpty()
                             ? tr("Project")
                             : m_state->document().metadata.name;
    setWindowTitle(isModified() ? tr("%1 — edited").arg(name) : name);
}

void ProjectWindow::updateViewerInfo()
{
    if (!m_viewerInfoLabel)
        return;

    const Document &document = m_state->document();
    m_viewerInfoLabel->setText(QStringLiteral("%1 × %2  ·  %3 fps")
                                   .arg(document.render.width)
                                   .arg(document.render.height)
                                   .arg(document.render.fps));
}

void ProjectWindow::updateTransport()
{
    if (!m_timeLabel)
        return;

    const int fps = qMax(1, m_state->document().render.fps);
    m_timeLabel->setText(QStringLiteral("%1  /  %2")
                             .arg(formatTimecode(m_state->playhead(), fps),
                                  formatTimecode(m_state->timelineDuration(), fps)));
}

void ProjectWindow::togglePlayback()
{
    if (m_playing) {
        stopPlayback();
        return;
    }

    // Playing from the very end would sit there; start again instead.
    if (m_state->playhead() >= m_state->timelineDuration() - 1e-6)
        m_state->setPlayhead(0.0);

    m_playing = true;
    m_playButton->setText(QStringLiteral("❚❚"));
    m_playButton->setToolTip(tr("Pause"));
    m_playbackTimer->start(1000 / qMax(1, m_state->document().render.fps));

    // The scene's sounds run alongside the picture, from wherever it starts.
    m_audio->setPlaying(true, m_state->document().timeline, m_state->playhead());
}

void ProjectWindow::stopPlayback()
{
    if (!m_playing)
        return;
    m_playing = false;
    m_playbackTimer->stop();
    m_playButton->setText(QStringLiteral("▶"));
    m_playButton->setToolTip(tr("Play"));
    m_audio->stop();
}

void ProjectWindow::stepFrame(int frames)
{
    stopPlayback();
    const double step = double(frames) / qMax(1, m_state->document().render.fps);
    m_state->setPlayhead(m_state->playhead() + step);
}

void ProjectWindow::advancePlayback()
{
    const double step = 1.0 / qMax(1, m_state->document().render.fps);
    const double next = m_state->playhead() + step;

    if (next >= m_state->timelineDuration()) {
        m_state->setPlayhead(m_state->timelineDuration());
        stopPlayback();
        return;
    }
    m_state->setPlayhead(next);
}

void ProjectWindow::addSound()
{
    const QString chosen =
        QFileDialog::getOpenFileName(this, tr("Add Sound"), m_layout.assetsDir,
                                     tr("Audio (*.wav *.mp3 *.m4a *.aiff *.flac *.ogg)"));
    if (chosen.isEmpty())
        return;

    if (m_state->addAudio(chosen) == kInvalidClipId) {
        QMessageBox::warning(this, tr("Cannot add that"),
                             tr("The file could not be copied into the project's assets."));
    }
}

void ProjectWindow::revealProjectFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_layout.root));
}

bool ProjectWindow::confirmDiscard()
{
    if (!isModified())
        return true;

    QMessageBox box(QMessageBox::Question, tr("Save changes?"),
                    tr("%1 has unsaved changes.").arg(m_state->document().metadata.name),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    box.setDefaultButton(QMessageBox::Save);

    switch (box.exec()) {
    case QMessageBox::Save:
        return save();
    case QMessageBox::Discard:
        return true;
    default:
        return false;
    }
}

void ProjectWindow::closeEvent(QCloseEvent *event)
{
    stopPlayback();
    if (!confirmDiscard()) {
        event->ignore();
        return;
    }
    saveWindowState();
    event->accept();
    Q_EMIT closed();
}

} // namespace mn::ui
