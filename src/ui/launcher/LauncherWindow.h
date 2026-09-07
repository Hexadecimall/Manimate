#pragma once

#include "AppWindow.h"

class QLabel;
class QLineEdit;
class QListView;
class QStackedWidget;

namespace mn::ui {

class RecentProjectsModel;
class RecentProjectsFilter;

/// The window the application opens on: pick a recent project, open one from
/// disk, or create a new one. Opening a project hands off to the editor.
class LauncherWindow : public AppWindow
{
    Q_OBJECT

public:
    explicit LauncherWindow(QWidget *parent = nullptr);

Q_SIGNALS:
    /// A project was chosen. The path is to its .manproj file.
    void projectOpened(const QString &projectFile);

public Q_SLOTS:
    void newProject();
    void openProjectFromDisk();

    /// Open `path`, which may be a .manproj file or a project folder.
    void openProject(const QString &path);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void buildMenus();
    QWidget *buildSidebar();
    QWidget *buildProjectList();
    QWidget *buildEmptyState();

    void refreshRecents();
    void openSelected();
    void showContextMenu(const QPoint &position);
    void reportError(const QString &title, const QString &detail);
    void showAbout();

    RecentProjectsModel *m_model = nullptr;
    RecentProjectsFilter *m_filter = nullptr;
    QListView *m_list = nullptr;
    QLineEdit *m_search = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_countLabel = nullptr;
};

} // namespace mn::ui
