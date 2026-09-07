#pragma once

#include "RecentProjects.h"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

namespace mn::ui {

/// The recent-project list, as a model the launcher's view can render.
class RecentProjectsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        DescriptionRole,
        ProjectFileRole,
        RootDirRole,
        LastOpenedRole,
        MissingRole,
    };

    explicit RecentProjectsModel(QObject *parent = nullptr);

    /// Reload from settings, dropping entries whose files have disappeared
    /// only when `pruneMissing` is set; otherwise they are kept and flagged so
    /// the user can see what happened.
    void reload(bool pruneMissing = false);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    RecentProject entryAt(int row) const;
    void removeAt(int row);

private:
    QVector<RecentProject> m_entries;
};

/// Case-insensitive filter across project name, description and path.
class RecentProjectsFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit RecentProjectsFilter(QObject *parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
};

} // namespace mn::ui
