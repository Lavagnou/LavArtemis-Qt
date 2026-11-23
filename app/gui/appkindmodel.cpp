#include "appkindmodel.h"
#include "appmodel.h"

AppKindModel::AppKindModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);

    // QML sections show or hide on the row count
    connect(this, &QAbstractItemModel::rowsInserted, this, &AppKindModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &AppKindModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &AppKindModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &AppKindModel::countChanged);
}

void AppKindModel::setKind(const QString &kind)
{
    if (m_Kind != kind) {
        m_Kind = kind;
        invalidateFilter();
        emit kindChanged();
        emit countChanged();
    }
}

int AppKindModel::sourceRow(int proxyRow) const
{
    QModelIndex source = mapToSource(index(proxyRow, 0));
    return source.isValid() ? source.row() : -1;
}

bool AppKindModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_Kind.isEmpty()) {
        return true;
    }
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    return sourceModel()->data(index, AppModel::KindRole).toString() == m_Kind;
}
