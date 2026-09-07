#pragma once

#include <QString>
#include <optional>

namespace mn {

class Document;

/// Every path that makes up a project on disk.
///
///   My Project/
///     My Project.manproj    the document; the only irreplaceable file
///     Manimation/           derived state, safe to delete
///       cache/ intermediate/ backups/
///     export/               standalone Python, runnable with `manim`
///     output/               rendered video
///     assets/               images, audio, anything referenced by the scene
struct ProjectLayout
{
    QString root;
    QString projectFile;
    QString internalDir;
    QString cacheDir;
    QString intermediateDir;
    QString backupsDir;
    QString exportDir;
    QString outputDir;
    QString assetsDir;

    bool isValid() const { return !root.isEmpty(); }
};

namespace project {

/// File extension of a project document, without the dot.
inline constexpr auto kExtension = "manproj";

/// Name of the folder holding derived state.
inline constexpr auto kInternalDirName = "Manimation";

/// Strip characters that are illegal or awkward in a folder name. Returns an
/// empty string if nothing usable is left.
QString sanitizeName(const QString &name);

/// Turn a project name into a legal, lower_snake_case Python module name.
QString pythonModuleName(const QString &name);

/// Turn a name into a legal PascalCase Python class name.
QString pythonClassName(const QString &name);

/// Compute the layout a project of this name would have under `rootDir`.
/// Does not touch the filesystem.
ProjectLayout layoutFor(const QString &rootDir, const QString &name);

/// Create the full folder structure under `parentDir` and write a new
/// document into it. Fails if the target folder already exists.
bool create(const QString &parentDir, const QString &name, ProjectLayout *layoutOut,
            QString *errorOut = nullptr);

/// Resolve a layout from either a .manproj file path or a project folder path.
/// Returns nothing if no project document can be found there.
std::optional<ProjectLayout> resolve(const QString &path);

/// Recreate any missing derived folders. Called on open so a project whose
/// Manimation/ folder was deleted still works.
bool ensureDirectories(const ProjectLayout &layout, QString *errorOut = nullptr);

} // namespace project
} // namespace mn
