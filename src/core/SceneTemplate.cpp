#include "SceneTemplate.h"

#include "Document.h"
#include "Project.h"

#include <QDir>
#include <QSaveFile>
#include <QTextStream>

namespace mn::scene_template {

QString starterScript(const Document &document)
{
    const QString sceneClass = document.sceneClassName.isEmpty()
                                   ? QStringLiteral("MainScene")
                                   : document.sceneClassName;
    const QString title = document.metadata.name.isEmpty() ? QStringLiteral("Manimation")
                                                           : document.metadata.name;

    // Escaped for a Python double-quoted string.
    QString safeTitle = title;
    safeTitle.replace(QLatin1String("\\"), QLatin1String("\\\\"));
    safeTitle.replace(QLatin1String("\""), QLatin1String("\\\""));

    return QStringLiteral(
               "from manim import *\n"
               "\n"
               "\n"
               "class %1(Scene):\n"
               "    def construct(self):\n"
               "        title = Text(\"%2\", font_size=48)\n"
               "        subtitle = Text(\"Made with Manimation\", font_size=24, color=BLUE_C)\n"
               "        subtitle.next_to(title, DOWN)\n"
               "\n"
               "        self.play(Write(title))\n"
               "        self.play(FadeIn(subtitle, shift=UP))\n"
               "        self.wait(2)\n")
        .arg(sceneClass, safeTitle);
}

QString scriptPathFor(const ProjectLayout &layout, const Document &document)
{
    const QDir exportDir(layout.exportDir);

    // A project built by importing a script already has one; prefer it over a
    // file named after the project, which may not exist.
    const QStringList existing = exportDir.entryList({QStringLiteral("*.py")}, QDir::Files, QDir::Name);
    if (!existing.isEmpty()) {
        const QString preferred = project::pythonModuleName(document.metadata.name) + QStringLiteral(".py");
        if (existing.contains(preferred))
            return exportDir.filePath(preferred);
        return exportDir.filePath(existing.first());
    }

    return exportDir.filePath(project::pythonModuleName(document.metadata.name) + QStringLiteral(".py"));
}

QString ensureScript(const ProjectLayout &layout, const Document &document, QString *errorOut)
{
    const QString path = scriptPathFor(layout, document);
    if (QFileInfo::exists(path))
        return path;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = file.errorString();
        return {};
    }

    QTextStream stream(&file);
    stream << starterScript(document);
    stream.flush();

    if (!file.commit()) {
        if (errorOut)
            *errorOut = file.errorString();
        return {};
    }
    return path;
}

} // namespace mn::scene_template
