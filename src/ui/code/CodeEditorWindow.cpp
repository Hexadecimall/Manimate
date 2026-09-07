#include "CodeEditorWindow.h"

#include "CodeEditor.h"
#include "Theme.h"
#include "TitleBar.h"

#include <QAction>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QSaveFile>
#include <QTextStream>
#include <QVBoxLayout>

namespace mn::ui {
namespace {

QString scriptFilter()
{
    return QObject::tr("Python script (*.py)");
}

} // namespace

CodeEditorWindow::CodeEditorWindow(QWidget *parent)
    : AppWindow(parent)
{
    const theme::Palette &p = theme::palette();

    resize(1000, 720);
    setMinimumSize(560, 360);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_editor = new CodeEditor;
    layout->addWidget(m_editor, 1);

    // A thin status strip: where the file is, and where the cursor is.
    auto *status = new QWidget;
    status->setObjectName(QStringLiteral("status"));
    status->setAttribute(Qt::WA_StyledBackground, true);
    status->setFixedHeight(26);
    status->setStyleSheet(
        QStringLiteral("QWidget#status { background: %1; border-top: 1px solid %2; }")
            .arg(p.window.name(), p.border.name()));

    m_pathLabel = new QLabel;
    m_pathLabel->setProperty("role", "subtitle");
    m_positionLabel = new QLabel;
    m_positionLabel->setProperty("role", "subtitle");

    auto *statusLayout = new QHBoxLayout(status);
    statusLayout->setContentsMargins(14, 0, 14, 0);
    statusLayout->addWidget(m_pathLabel, 1);
    statusLayout->addWidget(m_positionLabel);
    layout->addWidget(status);

    setContent(content);
    buildMenus();

    connect(m_editor, &CodeEditor::cursorPositionDescribed, this, &CodeEditorWindow::updatePosition);
    connect(m_editor->document(), &QTextDocument::modificationChanged,
            this, [this] { updateTitle(); });

    // Setting up the editor and running the highlighter over an empty document
    // can leave it flagged as modified; nothing has actually been edited yet.
    m_editor->document()->setModified(false);

    updateTitle();
    updatePosition(1, 1);
    m_editor->setFocus();
}

void CodeEditorWindow::buildMenus()
{
    auto *fileMenu = new QMenu(tr("File"), this);

    QAction *openAction = fileMenu->addAction(tr("Open Script…"), QKeySequence::Open, this, [this] {
        const QString chosen =
            QFileDialog::getOpenFileName(this, tr("Open Script"), QFileInfo(m_filePath).absolutePath(),
                                         scriptFilter());
        if (!chosen.isEmpty() && confirmDiscard())
            open(chosen);
    });
    QAction *saveAction = fileMenu->addAction(tr("Save"), QKeySequence::Save, this,
                                              [this] { save(); });
    QAction *saveAsAction = fileMenu->addAction(tr("Save As…"), QKeySequence::SaveAs, this,
                                                [this] { saveAs(); });
    fileMenu->addSeparator();
    QAction *closeAction = fileMenu->addAction(tr("Close"), QKeySequence::Close, this,
                                               [this] { close(); });

    for (QAction *action : {openAction, saveAction, saveAsAction, closeAction})
        action->setMenuRole(QAction::NoRole);

    setMenus({fileMenu});
}

bool CodeEditorWindow::isModified() const
{
    return m_editor->document()->isModified();
}

void CodeEditorWindow::updateTitle()
{
    const QString name = m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName();
    setWindowTitle(isModified() ? tr("%1 — edited").arg(name) : name);

    m_pathLabel->setText(m_filePath.isEmpty() ? tr("Not saved yet")
                                              : QDir::toNativeSeparators(m_filePath));
}

void CodeEditorWindow::updatePosition(int line, int column)
{
    m_positionLabel->setText(tr("Line %1, Column %2  ·  %3 lines")
                                 .arg(line)
                                 .arg(column)
                                 .arg(m_editor->lineCount()));
}

bool CodeEditorWindow::open(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Cannot open that"), file.errorString());
        return false;
    }

    QTextStream stream(&file);
    m_editor->setPlainText(stream.readAll());
    m_editor->document()->setModified(false);

    m_filePath = QFileInfo(filePath).absoluteFilePath();
    updateTitle();
    updatePosition(1, 1);
    return true;
}

bool CodeEditorWindow::writeTo(const QString &filePath)
{
    QSaveFile file(filePath);
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

    m_filePath = QFileInfo(filePath).absoluteFilePath();
    m_editor->document()->setModified(false);
    updateTitle();
    return true;
}

bool CodeEditorWindow::save()
{
    if (m_filePath.isEmpty())
        return saveAs();
    return writeTo(m_filePath);
}

bool CodeEditorWindow::saveAs()
{
    const QString chosen = QFileDialog::getSaveFileName(
        this, tr("Save Script"),
        m_filePath.isEmpty() ? QStringLiteral("scene.py") : m_filePath, scriptFilter());
    if (chosen.isEmpty())
        return false;
    return writeTo(chosen);
}

bool CodeEditorWindow::confirmDiscard()
{
    if (!isModified())
        return true;

    QMessageBox box(QMessageBox::Question, tr("Save changes?"),
                    tr("%1 has unsaved changes.")
                        .arg(m_filePath.isEmpty() ? tr("This script")
                                                  : QFileInfo(m_filePath).fileName()),
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

void CodeEditorWindow::closeEvent(QCloseEvent *event)
{
    if (confirmDiscard())
        event->accept();
    else
        event->ignore();
}

} // namespace mn::ui
