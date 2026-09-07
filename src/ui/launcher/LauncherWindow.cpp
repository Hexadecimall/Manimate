#include "LauncherWindow.h"

#include "Document.h"
#include "NewProjectDialog.h"
#include "Project.h"
#include "ProjectCardDelegate.h"
#include "RecentProjectsModel.h"
#include "Theme.h"
#include "TitleBar.h"
#include "Version.h"
#include "Wordmark.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace mn::ui {
namespace {

constexpr int kSidebarWidth = 340;

QString projectFilter()
{
    return QObject::tr("Manimation Project (*.%1)").arg(QLatin1String(project::kExtension));
}

QString describeCount(int count)
{
    if (count == 0)
        return QObject::tr("No matches");
    if (count == 1)
        return QObject::tr("1 project");
    return QObject::tr("%1 projects").arg(count);
}

QPushButton *sidebarButton(const QString &text, const QString &role)
{
    auto *button = new QPushButton(text);
    button->setProperty("role", role);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(38);
    return button;
}

} // namespace

LauncherWindow::LauncherWindow(QWidget *parent)
    : AppWindow(parent)
{
    setWindowTitle(tr("Manimation"));
    setAcceptDrops(true);
    resize(1080, 700);
    setMinimumSize(880, 560);

    auto *central = new QWidget;
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(buildSidebar());
    layout->addWidget(buildProjectList(), 1);
    setContent(central);

    buildMenus();
    refreshRecents();
}

void LauncherWindow::buildMenus()
{
    auto *fileMenu = new QMenu(tr("File"), this);
    QAction *newAction = fileMenu->addAction(tr("New Project…"), QKeySequence::New, this,
                                             &LauncherWindow::newProject);
    QAction *openAction = fileMenu->addAction(tr("Open Project…"), QKeySequence::Open, this,
                                              &LauncherWindow::openProjectFromDisk);
    newAction->setMenuRole(QAction::NoRole);
    openAction->setMenuRole(QAction::NoRole);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

    auto *helpMenu = new QMenu(tr("Help"), this);
    helpMenu->addAction(tr("About Manimation"), this, &LauncherWindow::showAbout);

    setMenus({fileMenu, helpMenu});
}

void LauncherWindow::showAbout()
{
    QMessageBox box(QMessageBox::NoIcon, tr("About Manimation"), tr("Manimation %1").arg(version::string()),
                    QMessageBox::Ok, this);
    box.setInformativeText(tr("A visual editor for Manim."));
    box.exec();
}

QWidget *LauncherWindow::buildSidebar()
{
    const theme::Palette &p = theme::palette();

    auto *sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(kSidebarWidth);
    // Scoped to the object name so the rule cannot leak onto child frames.
    sidebar->setStyleSheet(QStringLiteral("QFrame#sidebar { background: %1; border-right: 1px solid %2; }")
                               .arg(p.surface.name(), p.border.name()));

    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(28, 34, 28, 24);
    layout->setSpacing(0);

    auto *wordmark = new Wordmark;
    wordmark->setGlyphSize(30);
    wordmark->setSubtitle(tr("VISUAL EDITOR FOR MANIM"));
    layout->addWidget(wordmark);

    layout->addSpacing(38);

    auto *newButton = sidebarButton(tr("New Project"), QStringLiteral("primary"));
    auto *openButton = sidebarButton(tr("Open Project…"), QString());
    layout->addWidget(newButton);
    layout->addSpacing(10);
    layout->addWidget(openButton);

    layout->addStretch(1);

    auto *separator = new QFrame;
    separator->setObjectName(QStringLiteral("hairline"));
    separator->setFixedHeight(1);
    separator->setStyleSheet(
        QStringLiteral("QFrame#hairline { background: %1; border: none; }").arg(p.border.name()));
    layout->addWidget(separator);
    layout->addSpacing(14);

    auto *versionLabel = new QLabel(tr("Version %1").arg(version::string()));
    versionLabel->setProperty("role", "subtitle");
    layout->addWidget(versionLabel);

    connect(newButton, &QPushButton::clicked, this, &LauncherWindow::newProject);
    connect(openButton, &QPushButton::clicked, this, &LauncherWindow::openProjectFromDisk);

    return sidebar;
}

QWidget *LauncherWindow::buildProjectList()
{
    auto *panel = new QWidget;
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(30, 34, 30, 24);
    layout->setSpacing(0);

    auto *header = new QHBoxLayout;
    header->setSpacing(16);

    auto *heading = new QVBoxLayout;
    heading->setSpacing(2);
    auto *title = new QLabel(tr("Projects"));
    title->setProperty("role", "title");
    m_countLabel = new QLabel;
    m_countLabel->setProperty("role", "subtitle");
    heading->addWidget(title);
    heading->addWidget(m_countLabel);
    header->addLayout(heading);
    header->addStretch(1);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search projects"));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedWidth(240);
    header->addWidget(m_search, 0, Qt::AlignVCenter);

    layout->addLayout(header);
    layout->addSpacing(20);

    m_model = new RecentProjectsModel(this);
    m_filter = new RecentProjectsFilter(this);
    m_filter->setSourceModel(m_model);

    m_list = new QListView;
    m_list->setModel(m_filter);
    m_list->setItemDelegate(new ProjectCardDelegate(this));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setMouseTracking(true);
    m_list->setUniformItemSizes(true);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setCursor(Qt::PointingHandCursor);

    m_stack = new QStackedWidget;
    m_stack->addWidget(m_list);
    m_stack->addWidget(buildEmptyState());
    layout->addWidget(m_stack, 1);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_filter->setFilterFixedString(text);
        m_countLabel->setText(describeCount(m_filter->rowCount()));
    });
    connect(m_list, &QListView::activated, this, &LauncherWindow::openSelected);
    connect(m_list, &QListView::customContextMenuRequested, this, &LauncherWindow::showContextMenu);

    return panel;
}

QWidget *LauncherWindow::buildEmptyState()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addStretch(1);

    auto *title = new QLabel(tr("No projects yet"));
    title->setAlignment(Qt::AlignCenter);
    QFont titleFont = theme::font(1, QFont::DemiBold);
    titleFont.setPixelSize(16);
    title->setFont(titleFont);

    auto *hint = new QLabel(tr("Create one to start building a scene, or drop a .%1 file here.")
                                .arg(QLatin1String(project::kExtension)));
    hint->setProperty("role", "subtitle");
    hint->setAlignment(Qt::AlignCenter);

    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addStretch(2);
    return page;
}

void LauncherWindow::refreshRecents()
{
    m_model->reload();
    const int count = m_model->rowCount();
    m_countLabel->setText(count == 0 ? tr("Nothing here yet") : describeCount(count));
    m_stack->setCurrentIndex(count == 0 ? 1 : 0);
    if (count > 0)
        m_list->setCurrentIndex(m_filter->index(0, 0));
}

void LauncherWindow::newProject()
{
    NewProjectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    openProject(dialog.layout().projectFile);
}

void LauncherWindow::openProjectFromDisk()
{
    const QString chosen = QFileDialog::getOpenFileName(this, tr("Open Project"),
                                                        NewProjectDialog::defaultLocation(),
                                                        projectFilter());
    if (!chosen.isEmpty())
        openProject(chosen);
}

void LauncherWindow::openProject(const QString &path)
{
    const auto layout = project::resolve(path);
    if (!layout) {
        reportError(tr("Cannot open that"),
                    tr("%1 is not a Manimation project.").arg(QDir::toNativeSeparators(path)));
        return;
    }

    QString error;
    if (!project::ensureDirectories(*layout, &error)) {
        reportError(tr("Cannot open that"), error);
        return;
    }

    Document document;
    if (!Document::load(layout->projectFile, &document, &error)) {
        reportError(tr("Cannot open that"), error);
        return;
    }

    RecentProjects::touch(layout->projectFile, document.metadata.name, document.metadata.description);
    refreshRecents();
    Q_EMIT projectOpened(layout->projectFile);
}

void LauncherWindow::openSelected()
{
    const QModelIndex index = m_list->currentIndex();
    if (!index.isValid())
        return;
    openProject(index.data(RecentProjectsModel::ProjectFileRole).toString());
}

void LauncherWindow::showContextMenu(const QPoint &position)
{
    const QModelIndex index = m_list->indexAt(position);
    if (!index.isValid())
        return;

    const QString projectFile = index.data(RecentProjectsModel::ProjectFileRole).toString();
    const bool missing = index.data(RecentProjectsModel::MissingRole).toBool();

    QMenu menu(this);
    QAction *open = menu.addAction(tr("Open"));
    open->setEnabled(!missing);
    QAction *reveal = menu.addAction(tr("Show in File Manager"));
    reveal->setEnabled(!missing);
    menu.addSeparator();
    QAction *forget = menu.addAction(tr("Remove from List"));

    QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(position));
    if (chosen == open) {
        openProject(projectFile);
    } else if (chosen == reveal) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(projectFile).absolutePath()));
    } else if (chosen == forget) {
        m_model->removeAt(m_filter->mapToSource(index).row());
        refreshRecents();
    }
}

void LauncherWindow::reportError(const QString &title, const QString &detail)
{
    QMessageBox box(QMessageBox::Warning, title, title, QMessageBox::Ok, this);
    box.setInformativeText(detail);
    box.exec();
}

void LauncherWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls())
        return;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (project::resolve(url.toLocalFile())) {
            event->acceptProposedAction();
            return;
        }
    }
}

void LauncherWindow::dropEvent(QDropEvent *event)
{
    for (const QUrl &url : event->mimeData()->urls()) {
        if (project::resolve(url.toLocalFile())) {
            event->acceptProposedAction();
            openProject(url.toLocalFile());
            return;
        }
    }
}

} // namespace mn::ui
