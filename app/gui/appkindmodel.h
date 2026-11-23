#pragma once

#include <QSortFilterProxyModel>

// The redesigned home (gui/v2/HomeView.qml) shows an AppModel in sections: the Desktop and
// Playnite cards, the user's other apps, then the Playnite library. This proxy keeps the rows of
// one AppModel "kind" (see AppModel::KindRole); sourceRow() maps back for createSessionForApp().
class AppKindModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY kindChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit AppKindModel(QObject *parent = nullptr);

    QString kind() const { return m_Kind; }
    void setKind(const QString &kind);

    Q_INVOKABLE int sourceRow(int proxyRow) const;

    int count() const { return rowCount(); }

signals:
    void kindChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_Kind;
};
