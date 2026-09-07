#include "RecentProjects.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace mn {
namespace {

constexpr auto kArrayKey = "recentProjects";

} // namespace

bool RecentProject::exists() const
{
    return QFileInfo::exists(projectFile);
}

QString RecentProject::rootDir() const
{
    return QFileInfo(projectFile).absolutePath();
}

QVector<RecentProject> RecentProjects::load()
{
    QVector<RecentProject> entries;
    QSettings settings;
    const int count = settings.beginReadArray(QLatin1String(kArrayKey));
    entries.reserve(count);
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        RecentProject entry;
        entry.projectFile = settings.value(QStringLiteral("file")).toString();
        entry.name = settings.value(QStringLiteral("name")).toString();
        entry.description = settings.value(QStringLiteral("description")).toString();
        entry.lastOpened = settings.value(QStringLiteral("lastOpened")).toDateTime();
        if (!entry.projectFile.isEmpty())
            entries.append(entry);
    }
    settings.endArray();
    return entries;
}

void RecentProjects::save(const QVector<RecentProject> &entries)
{
    QSettings settings;
    settings.beginWriteArray(QLatin1String(kArrayKey));
    const int count = int(qMin<qsizetype>(entries.size(), kMaxEntries));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("file"), entries.at(i).projectFile);
        settings.setValue(QStringLiteral("name"), entries.at(i).name);
        settings.setValue(QStringLiteral("description"), entries.at(i).description);
        settings.setValue(QStringLiteral("lastOpened"), entries.at(i).lastOpened);
    }
    settings.endArray();
}

void RecentProjects::touch(const QString &projectFile, const QString &name, const QString &description)
{
    const QString canonical = QFileInfo(projectFile).absoluteFilePath();

    QVector<RecentProject> entries = load();
    entries.removeIf([&canonical](const RecentProject &entry) {
        return QFileInfo(entry.projectFile).absoluteFilePath() == canonical;
    });

    RecentProject entry;
    entry.projectFile = canonical;
    entry.name = name;
    entry.description = description;
    entry.lastOpened = QDateTime::currentDateTime();
    entries.prepend(entry);

    if (entries.size() > kMaxEntries)
        entries.resize(kMaxEntries);
    save(entries);
}

void RecentProjects::remove(const QString &projectFile)
{
    const QString canonical = QFileInfo(projectFile).absoluteFilePath();
    QVector<RecentProject> entries = load();
    entries.removeIf([&canonical](const RecentProject &entry) {
        return QFileInfo(entry.projectFile).absoluteFilePath() == canonical;
    });
    save(entries);
}

QVector<RecentProject> RecentProjects::pruneMissing()
{
    QVector<RecentProject> entries = load();
    const qsizetype before = entries.size();
    entries.removeIf([](const RecentProject &entry) { return !entry.exists(); });
    if (entries.size() != before)
        save(entries);
    return entries;
}

} // namespace mn
