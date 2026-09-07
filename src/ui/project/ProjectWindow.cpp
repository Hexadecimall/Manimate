#include "ProjectWindow.h"

#include "CodeEditor.h"
#include "RecentProjects.h"
#include "SceneTemplate.h"
#include "Theme.h"
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
#include <QSaveFile>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

namespace mn::ui {

ProjectWindow::ProjectWindow(QWidget *parent)
    : AppWindow(parent)
{
    const theme::Palette &p = theme::palette();

    resize(1100, 760);
    setMinimumSize(640, 420);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_editor = new CodeEditor;
    layout->addWidget(m_editor, 1);

    auto *status = new QWidget;
    status->setFixedHeight(26);
    status->setStyleSheet(QStringLiteral("QWidget { background: %1; border-top: 1px solid %2; }")
                              .arg(p.window.name(), p.border.name()));

    m_scriptLabel = new QLabel;
    m_scriptLabel->setProperty("role", "subtitle");
    m_positionLabel = new QLabel;
    m_positionLabel->setProperty("role", "subtitle");

    auto *statusLayout = new QHBoxLayout(status);
    statusLayout->setContentsMargins(14, 0, 14, 0);
    statusLayout->addWidget(m_scriptLabel, 1);
    statusLayout->addWidget(m_positionLabel);
    layout->addWidget(status);

    setContent(content);
    buildMenus();

    connect(m_editor, &CodeEditor::cursorPositionDescribed, this, &ProjectWindow::updateStatus);
    connect(m_editor->document(), &QTextDocument::modificationChanged, this,
            [this] { updateTitle(); });

    m_editor->document()->setModified(false);
    updateStatus(1, 1);
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

    for (QAction *action : {saveAction, revealAction, closeAction})
        action->setMenuRole(QAction::NoRole);

    setMenus({fileMenu});
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
    if (!project::ensureDirectories(m_layout, &error)
        || !Document::load(m_layout.projectFile, &m_document, &error)) {
        QMessageBox::warning(this, tr("Cannot open that"), error);
        return false;
    }

    // A project with no script yet gets one, so the editor always has
    // something real to open.
    m_scriptPath = scene_template::ensureScript(m_layout, m_document, &error);
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
    m_editor->setPlainText(stream.readAll());
    m_editor->document()->setModified(false);

    RecentProjects::touch(m_layout.projectFile, m_document.metadata.name,
                          m_document.metadata.description);

    updateTitle();
    updateStatus(1, 1);
    m_editor->setFocus();
    return true;
}

bool ProjectWindow::isModified() const
{
    return m_editor->document()->isModified();
}

void ProjectWindow::updateTitle()
{
    const QString name = m_document.metadata.name.isEmpty() ? tr("Project")
                                                            : m_document.metadata.name;
    setWindowTitle(isModified() ? tr("%1 — edited").arg(name) : name);

    m_scriptLabel->setText(m_scriptPath.isEmpty()
                               ? QString()
                               : QDir::toNativeSeparators(
                                     QDir(m_layout.root).relativeFilePath(m_scriptPath)));
}

void ProjectWindow::updateStatus(int line, int column)
{
    m_positionLabel->setText(tr("Line %1, Column %2  ·  %3 lines")
                                 .arg(line)
                                 .arg(column)
                                 .arg(m_editor->lineCount()));
}

bool ProjectWindow::save()
{
    if (m_scriptPath.isEmpty())
        return false;

    QSaveFile file(m_scriptPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Cannot save that"), file.errorString());
        return false;
    }

    QTextStream stream(&file);
    stream << m_editor->toPlainText();
    stream.flush();
    if (!file.commit()) {
        QMessageBox::warning(this, tr("Cannot save that"), file.errorString());
        return false;
    }

    m_editor->document()->setModified(false);
    updateTitle();

    // Saving the script is also a change to the project, so its own file is
    // touched to keep the modified time and revision honest.
    QString error;
    m_document.save(m_layout.projectFile, &error);
    return true;
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
                    tr("%1 has unsaved changes.").arg(m_document.metadata.name),
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
    if (!confirmDiscard()) {
        event->ignore();
        return;
    }
    event->accept();
    Q_EMIT closed();
}

} // namespace mn::ui
