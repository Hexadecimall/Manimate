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

namespace {

/// The list was written under the application's former name. Carried over the
/// first time the new one runs, so renaming does not look like losing every
/// project you had open.
void migrateFromFormerName()
{
    QSettings settings;
    if (settings.contains(QStringLiteral("%1/size").arg(QLatin1String(kArrayKey))))
        return;
    if (settings.value(QStringLiteral("migratedRecents")).toBool())
        return;

    QSettings former(QSettings::NativeFormat, QSettings::UserScope,
                     QStringLiteral("Manimation"), QStringLiteral("Manimation"));
    const int count = former.beginReadArray(QLatin1String(kArrayKey));
    if (count > 0) {
        QVector<RecentProject> carried;
        for (int i = 0; i < count; ++i) {
            former.setArrayIndex(i);
            RecentProject entry;
            entry.projectFile = former.value(QStringLiteral("file")).toString();
            entry.name = former.value(QStringLiteral("name")).toString();
            entry.description = former.value(QStringLiteral("description")).toString();
            entry.lastOpened = former.value(QStringLiteral("lastOpened")).toDateTime();
            if (!entry.projectFile.isEmpty())
                carried.append(entry);
        }
        former.endArray();
        if (!carried.isEmpty())
            RecentProjects::save(carried);
    } else {
        former.endArray();
    }

    settings.setValue(QStringLiteral("migratedRecents"), true);
}

} // namespace

QVector<RecentProject> RecentProjects::load()
{
    migrateFromFormerName();

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
