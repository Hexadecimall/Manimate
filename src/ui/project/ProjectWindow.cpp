#include "ProjectWindow.h"

#include "CanvasView.h"
#include "CodeEditor.h"
#include "EditorState.h"
#include "InspectorPanel.h"
#include "LibraryPanel.h"
#include "RecentProjects.h"
#include "SceneTemplate.h"
#include "Theme.h"
#include "TimelineView.h"
#include "TitleBar.h"

#include <QAction>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
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
    connect(m_playbackTimer, &QTimer::timeout, this, &ProjectWindow::advancePlayback);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_pages = new QStackedWidget;
    m_pages->addWidget(buildEditPage());
    m_pages->addWidget(buildCodePage());
    layout->addWidget(m_pages, 1);
    layout->addWidget(buildPageBar());

    setContent(content);
    buildMenus();

    connect(m_state, &EditorState::modifiedChanged, this, [this] { updateTitle(); });
    connect(m_state, &EditorState::playheadChanged, this, [this] { updateTransport(); });
    connect(m_state, &EditorState::documentChanged, this, [this] { updateTransport(); });
    connect(m_state, &EditorState::historyChanged, this, &ProjectWindow::updateHistoryActions);

    showPage(Page::Edit);
    updateTitle();
    updateTransport();
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

    m_canvas = new CanvasView(m_state);
    centreLayout->addWidget(m_canvas, 1);
    centreLayout->addWidget(buildTransportBar());

    auto *library = new LibraryPanel(m_state);
    library->setMinimumWidth(180);
    m_inspector = new InspectorPanel(m_state);
    m_inspector->setMinimumWidth(220);

    auto *upper = new QSplitter(Qt::Horizontal);
    upper->setHandleWidth(1);
    upper->setChildrenCollapsible(false);
    upper->addWidget(library);
    upper->addWidget(centre);
    upper->addWidget(m_inspector);
    upper->setStretchFactor(0, 0);
    upper->setStretchFactor(1, 1);
    upper->setStretchFactor(2, 0);
    upper->setSizes({kLibraryWidth, 900, kInspectorWidth});

    m_timeline = new TimelineView(m_state);

    auto *split = new QSplitter(Qt::Vertical);
    split->setHandleWidth(1);
    split->setChildrenCollapsible(false);
    split->addWidget(upper);
    split->addWidget(m_timeline);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    split->setSizes({620, 260});
    split->setStyleSheet(QStringLiteral("QSplitter::handle { background: %1; }").arg(p.border.name()));
    upper->setStyleSheet(QStringLiteral("QSplitter::handle { background: %1; }").arg(p.border.name()));

    layout->addWidget(split, 1);
    return page;
}

QWidget *ProjectWindow::buildTransportBar()
{
    const theme::Palette &p = theme::palette();

    auto *bar = new QWidget;
    bar->setFixedHeight(40);
    bar->setStyleSheet(QStringLiteral("QWidget { background: %1; border-top: 1px solid %2; }")
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

    auto *guides = new QPushButton(tr("Guides"));
    guides->setProperty("role", "quiet");
    guides->setCheckable(true);
    guides->setChecked(true);
    guides->setCursor(Qt::PointingHandCursor);
    connect(guides, &QPushButton::toggled, this,
            [this](bool on) { m_canvas->setGuidesVisible(on); });
    layout->addWidget(guides);

    return bar;
}

QWidget *ProjectWindow::buildCodePage()
{
    const theme::Palette &p = theme::palette();

    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_codeEditor = new CodeEditor;
    layout->addWidget(m_codeEditor, 1);

    auto *status = new QWidget;
    status->setFixedHeight(26);
    status->setStyleSheet(QStringLiteral("QWidget { background: %1; border-top: 1px solid %2; }")
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
    connect(m_codeEditor->document(), &QTextDocument::modificationChanged, this,
            [this] { updateTitle(); });
    connect(this, &ProjectWindow::closed, scriptLabel, [] {});

    // Kept up to date whenever a project is opened.
    scriptLabel->setObjectName(QStringLiteral("scriptLabel"));
    return page;
}

QWidget *ProjectWindow::buildPageBar()
{
    const theme::Palette &p = theme::palette();

    auto *bar = new QWidget;
    bar->setFixedHeight(kPageBarHeight);
    bar->setStyleSheet(QStringLiteral("QWidget { background: %1; border-top: 1px solid %2; }")
                           .arg(p.surface.name(), p.border.name()));

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
                     theme::mix(p.surfaceHover, p.accent, 0.30).name(), p.text.name()));
        return button;
    };

    m_editPageButton = makePageButton(tr("Edit"));
    m_codePageButton = makePageButton(tr("Code"));

    connect(m_editPageButton, &QPushButton::clicked, this, [this] { showPage(Page::Edit); });
    connect(m_codePageButton, &QPushButton::clicked, this, [this] { showPage(Page::Code); });

    layout->addStretch(1);
    layout->addWidget(m_editPageButton);
    layout->addWidget(m_codePageButton);
    layout->addStretch(1);

    return bar;
}

ProjectWindow::Page ProjectWindow::currentPage() const
{
    return m_pages->currentIndex() == 0 ? Page::Edit : Page::Code;
}

void ProjectWindow::showPage(Page page)
{
    m_pages->setCurrentIndex(page == Page::Edit ? 0 : 1);
    m_editPageButton->setChecked(page == Page::Edit);
    m_codePageButton->setChecked(page == Page::Code);

    if (page == Page::Edit)
        stopPlayback();
}

void ProjectWindow::buildMenus()
{
    auto *fileMenu = new QMenu(tr("File"), this);
    QAction *saveAction = fileMenu->addAction(tr("Save"), QKeySequence::Save, this, [this] { save(); });
    QAction *revealAction = fileMenu->addAction(tr("Show Project Folder"), this,
                                                &ProjectWindow::revealProjectFolder);
    fileMenu->addSeparator();
    QAction *closeAction = fileMenu->addAction(tr("Close Project"), QKeySequence::Close, this,
                                               [this] { close(); });

    auto *editMenu = new QMenu(tr("Edit"), this);
    m_undoAction = editMenu->addAction(tr("Undo"), QKeySequence::Undo, m_state, &EditorState::undo);
    m_redoAction = editMenu->addAction(tr("Redo"), QKeySequence::Redo, m_state, &EditorState::redo);
    editMenu->addSeparator();
    QAction *deleteAction = editMenu->addAction(tr("Delete"), QKeySequence::Delete, m_state,
                                                &EditorState::deleteSelection);

    auto *viewMenu = new QMenu(tr("View"), this);
    QAction *editPage = viewMenu->addAction(tr("Edit"), QKeySequence(tr("Ctrl+1")), this,
                                            [this] { showPage(Page::Edit); });
    QAction *codePage = viewMenu->addAction(tr("Code"), QKeySequence(tr("Ctrl+2")), this,
                                            [this] { showPage(Page::Code); });
    viewMenu->addSeparator();
    QAction *playAction = viewMenu->addAction(tr("Play / Pause"), QKeySequence(Qt::Key_Space), this,
                                              &ProjectWindow::togglePlayback);

    for (QAction *action : {saveAction, revealAction, closeAction, m_undoAction, m_redoAction,
                            deleteAction, editPage, codePage, playAction}) {
        action->setMenuRole(QAction::NoRole);
    }

    setMenus({fileMenu, editMenu, viewMenu});
    updateHistoryActions();
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
                             tr("%1 is not a Manimation project.")
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

    m_state->setDocument(std::move(document), m_layout);

    RecentProjects::touch(m_layout.projectFile, m_state->document().metadata.name,
                          m_state->document().metadata.description);

    updateTitle();
    updateTransport();
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
}

void ProjectWindow::stopPlayback()
{
    if (!m_playing)
        return;
    m_playing = false;
    m_playbackTimer->stop();
    m_playButton->setText(QStringLiteral("▶"));
    m_playButton->setToolTip(tr("Play"));
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
    event->accept();
    Q_EMIT closed();
}

} // namespace mn::ui
