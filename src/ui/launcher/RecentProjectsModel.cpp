#include "RecentProjectsModel.h"

#include <QDir>

namespace mn::ui {

RecentProjectsModel::RecentProjectsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    reload();
}

void RecentProjectsModel::reload(bool pruneMissing)
{
    beginResetModel();
    m_entries = pruneMissing ? RecentProjects::pruneMissing() : RecentProjects::load();
    endResetModel();
}

int RecentProjectsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant RecentProjectsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};

    const RecentProject &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return entry.name.isEmpty() ? QFileInfo(entry.projectFile).completeBaseName() : entry.name;
    case DescriptionRole:
        return entry.description;
    case ProjectFileRole:
        return entry.projectFile;
    case RootDirRole:
        return QDir::toNativeSeparators(entry.rootDir());
    case LastOpenedRole:
        return entry.lastOpened;
    case MissingRole:
        return !entry.exists();
    case Qt::ToolTipRole:
        return QDir::toNativeSeparators(entry.projectFile);
    default:
        return {};
    }
}

RecentProject RecentProjectsModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return {};
    return m_entries.at(row);
}

void RecentProjectsModel::removeAt(int row)
{
    if (row < 0 || row >= m_entries.size())
        return;
    RecentProjects::remove(m_entries.at(row).projectFile);
    beginRemoveRows({}, row, row);
    m_entries.remove(row);
    endRemoveRows();
}

RecentProjectsFilter::RecentProjectsFilter(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

bool RecentProjectsFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QString needle = filterRegularExpression().pattern();
    if (needle.isEmpty())
        return true;

    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const QStringList haystack = {
        index.data(RecentProjectsModel::NameRole).toString(),
        index.data(RecentProjectsModel::DescriptionRole).toString(),
        index.data(RecentProjectsModel::RootDirRole).toString(),
    };
    for (const QString &field : haystack) {
        if (field.contains(needle, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

} // namespace mn::ui
