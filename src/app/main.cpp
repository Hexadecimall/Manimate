#include "LauncherWindow.h"
#include "Theme.h"
#include "Version.h"

#include <QApplication>
#include <QCommandLineParser>

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
    launcher.show();

    const QStringList arguments = parser.positionalArguments();
    if (!arguments.isEmpty())
        launcher.openProject(arguments.first());

    return QApplication::exec();
}
