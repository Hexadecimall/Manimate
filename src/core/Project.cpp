#include "Project.h"

#include "Document.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace mn::project {
namespace {

QString join(const QString &base, const QString &leaf)
{
    return QDir(base).filePath(leaf);
}

bool makePath(const QString &path, QString *errorOut)
{
    if (QDir().mkpath(path))
        return true;
    if (errorOut)
        *errorOut = QStringLiteral("Could not create folder: %1").arg(QDir::toNativeSeparators(path));
    return false;
}

} // namespace

QString sanitizeName(const QString &name)
{
    static const QRegularExpression illegal(QStringLiteral(R"([\\/:*?"<>|\x00-\x1f])"));
    QString clean = name;
    clean.replace(illegal, QStringLiteral(" "));
    clean = clean.simplified();
    // A trailing dot or space makes a folder unusable on Windows.
    while (clean.endsWith(QLatin1Char('.')) || clean.endsWith(QLatin1Char(' ')))
        clean.chop(1);
    return clean;
}

QString pythonModuleName(const QString &name)
{
    QString result;
    result.reserve(name.size());
    for (const QChar c : name) {
        if (c.isLetterOrNumber())
            result.append(c.toLower());
        else if (!result.endsWith(QLatin1Char('_')))
            result.append(QLatin1Char('_'));
    }
    while (result.endsWith(QLatin1Char('_')))
        result.chop(1);
    if (result.isEmpty() || result.at(0).isDigit())
        result.prepend(QStringLiteral("scene_"));
    return result;
}

QString pythonClassName(const QString &name)
{
    QString result;
    bool capitalise = true;
    for (const QChar c : name) {
        if (c.isLetterOrNumber()) {
            result.append(capitalise ? c.toUpper() : c);
            capitalise = false;
        } else {
            capitalise = true;
        }
    }
    if (result.isEmpty() || result.at(0).isDigit())
        result.prepend(QStringLiteral("Scene"));
    return result;
}

ProjectLayout layoutFor(const QString &rootDir, const QString &name)
{
    ProjectLayout layout;
    layout.root = QDir::cleanPath(rootDir);
    layout.projectFile = join(layout.root, QStringLiteral("%1.%2").arg(name, QLatin1String(kExtension)));
    layout.internalDir = join(layout.root, QLatin1String(kInternalDirName));
    layout.cacheDir = join(layout.internalDir, QStringLiteral("cache"));
    layout.intermediateDir = join(layout.internalDir, QStringLiteral("intermediate"));
    layout.backupsDir = join(layout.internalDir, QStringLiteral("backups"));
    layout.exportDir = join(layout.root, QStringLiteral("export"));
    layout.outputDir = join(layout.root, QStringLiteral("output"));
    layout.assetsDir = join(layout.root, QStringLiteral("assets"));
    return layout;
}

bool ensureDirectories(const ProjectLayout &layout, QString *errorOut)
{
    // A project made before the rename keeps its derived state in a folder of
    // the old name. Move it rather than leaving the old one orphaned and
    // silently rebuilding everything beside it.
    const QString legacy = join(layout.root, QLatin1String(kLegacyInternalDirName));
    if (QFileInfo(legacy).isDir() && !QFileInfo(layout.internalDir).exists())
        QDir().rename(legacy, layout.internalDir);

    const QStringList directories = {
        layout.root,     layout.internalDir, layout.cacheDir,  layout.intermediateDir,
        layout.backupsDir, layout.exportDir, layout.outputDir, layout.assetsDir,
    };
    for (const QString &directory : directories) {
        if (!makePath(directory, errorOut))
            return false;
    }
    return true;
}

bool create(const QString &parentDir, const QString &name, ProjectLayout *layoutOut, QString *errorOut)
{
    const QString clean = sanitizeName(name);
    if (clean.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Give the project a name.");
        return false;
    }

    const QString root = join(parentDir, clean);
    if (QFileInfo::exists(root)) {
        if (errorOut)
            *errorOut = QStringLiteral("A folder called \"%1\" already exists here.").arg(clean);
        return false;
    }

    const ProjectLayout layout = layoutFor(root, clean);
    if (!ensureDirectories(layout, errorOut))
        return false;

    Document document = Document::createNew(clean);
    document.sceneClassName = pythonClassName(clean);
    if (!document.save(layout.projectFile, errorOut))
        return false;

    if (layoutOut)
        *layoutOut = layout;
    return true;
}

std::optional<ProjectLayout> resolve(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists())
        return std::nullopt;

    if (info.isFile()) {
        if (info.suffix().compare(QLatin1String(kExtension), Qt::CaseInsensitive) != 0)
            return std::nullopt;
        ProjectLayout layout = layoutFor(info.absolutePath(), info.completeBaseName());
        layout.projectFile = info.absoluteFilePath();
        return layout;
    }

    const QDir dir(info.absoluteFilePath());
    const QStringList matches =
        dir.entryList({QStringLiteral("*.%1").arg(QLatin1String(kExtension))}, QDir::Files, QDir::Name);
    if (matches.isEmpty())
        return std::nullopt;

    ProjectLayout layout = layoutFor(dir.absolutePath(), QFileInfo(matches.first()).completeBaseName());
    layout.projectFile = dir.absoluteFilePath(matches.first());
    return layout;
}

} // namespace mn::project
