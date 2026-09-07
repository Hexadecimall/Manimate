#include "LauncherWindow.h"
#include "ProjectWindow.h"
#include "Theme.h"
#include "Version.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QPointer>

namespace {

/// Open `projectFile` in a window of its own, standing the launcher down while
/// it is up. Closing the project brings the launcher back, so the application
/// always has somewhere to be.
void openProject(mn::ui::LauncherWindow *launcher, const QString &projectFile)
{
    auto *window = new mn::ui::ProjectWindow;
    window->setAttribute(Qt::WA_DeleteOnClose);

    if (!window->openProject(projectFile)) {
        delete window;
        launcher->show();
        launcher->raise();
        return;
    }

    QPointer<mn::ui::LauncherWindow> safeLauncher(launcher);
    QObject::connect(window, &mn::ui::ProjectWindow::closed, launcher, [safeLauncher] {
        if (!safeLauncher)
            return;
        safeLauncher->show();
        safeLauncher->raise();
        safeLauncher->activateWindow();
    });

    launcher->hide();
    window->show();
    window->raise();
    window->activateWindow();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Manimation"));
    QApplication::setApplicationVersion(mn::version::string());
    QApplication::setOrganizationName(QStringLiteral("Manimation"));
    QApplication::setOrganizationDomain(QStringLiteral("manimation.app"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Visual editor for Manim."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("project"),
                                 QStringLiteral("A .manproj file or project folder to open."));
    parser.process(application);

    mn::theme::apply(application);

    mn::ui::LauncherWindow launcher;
    QObject::connect(&launcher, &mn::ui::LauncherWindow::projectOpened, &launcher,
                     [&launcher](const QString &projectFile) { openProject(&launcher, projectFile); });
    launcher.show();

    const QStringList arguments = parser.positionalArguments();
    if (!arguments.isEmpty())
        launcher.openProject(arguments.first());

    return QApplication::exec();
}
