#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

namespace mn {

/// One entry in the launcher's project list.
struct RecentProject
{
    QString projectFile;
    QString name;
    QString description;
    QDateTime lastOpened;

    /// True if the file is still where it was last seen.
    bool exists() const;

    /// Folder containing the project.
    QString rootDir() const;
};

/// Persistent list of recently opened projects, newest first.
///
/// Stored in the application's own settings rather than inside any project, so
/// it survives moving or deleting individual projects.
class RecentProjects
{
public:
    static constexpr int kMaxEntries = 24;

    static QVector<RecentProject> load();
    static void save(const QVector<RecentProject> &entries);

    /// Record a project as just-opened, moving it to the front.
    static void touch(const QString &projectFile, const QString &name, const QString &description);

    static void remove(const QString &projectFile);

    /// Drop entries whose files are gone. Returns the surviving list.
    static QVector<RecentProject> pruneMissing();
};

} // namespace mn
