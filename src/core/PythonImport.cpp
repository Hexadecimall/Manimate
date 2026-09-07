#include "PythonImport.h"

#include "Document.h"
#include "Project.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

namespace mn::python_import {
namespace {

/// Class definitions, capturing the name and the base list.
const QRegularExpression &classPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^[ \t]*class[ \t]+([A-Za-z_]\w*)[ \t]*\(([^)]*)\)[ \t]*:)"),
        QRegularExpression::MultilineOption);
    return pattern;
}

const QRegularExpression &manimImportPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^[ \t]*(?:from[ \t]+manim(?:\.\w+)*[ \t]+import|import[ \t]+manim)\b)"),
        QRegularExpression::MultilineOption);
    return pattern;
}

/// Any base whose name ends in "Scene" is treated as a Manim scene, which
/// covers Scene, ThreeDScene, MovingCameraScene and anyone's own subclass.
bool basesNameAScene(const QString &bases)
{
    const QStringList parts = bases.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        QString base = part.trimmed();
        // Strip a module qualifier: manim.ThreeDScene -> ThreeDScene.
        const qsizetype dot = base.lastIndexOf(QLatin1Char('.'));
        if (dot >= 0)
            base = base.mid(dot + 1);
        if (base.endsWith(QLatin1String("Scene")))
            return true;
    }
    return false;
}

} // namespace

Scan scan(const QString &source)
{
    Scan result;
    result.lineCount = int(source.count(QLatin1Char('\n'))) + (source.isEmpty() ? 0 : 1);
    result.importsManim = manimImportPattern().match(source).hasMatch();

    auto matches = classPattern().globalMatch(source);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        if (basesNameAScene(match.captured(2)))
            result.sceneClasses.append(match.captured(1));
    }
    return result;
}

Scan scanFile(const QString &filePath, QString *errorOut)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = file.errorString();
        return {};
    }
    QTextStream stream(&file);
    return scan(stream.readAll());
}

bool into(const ProjectLayout &layout, const QString &sourceFile, Document *document, QString *errorOut)
{
    const QFileInfo info(sourceFile);
    if (!info.isFile()) {
        if (errorOut)
            *errorOut = QStringLiteral("%1 is not a file.").arg(QDir::toNativeSeparators(sourceFile));
        return false;
    }

    QString error;
    const Scan found = scanFile(sourceFile, &error);
    if (!error.isEmpty()) {
        if (errorOut)
            *errorOut = error;
        return false;
    }

    // The export folder is where runnable Python lives, so that is where an
    // imported script belongs.
    const QString moduleName = project::pythonModuleName(info.completeBaseName());
    const QString target = QDir(layout.exportDir).filePath(moduleName + QStringLiteral(".py"));

    QFile::remove(target);
    if (!QFile::copy(sourceFile, target)) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not copy the script into %1.")
                            .arg(QDir::toNativeSeparators(layout.exportDir));
        return false;
    }

    if (document && !found.sceneClasses.isEmpty())
        document->sceneClassName = found.sceneClasses.first();

    return true;
}

} // namespace mn::python_import
