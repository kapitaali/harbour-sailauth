#ifndef ACCOUNTFILTER_H
#define ACCOUNTFILTER_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>

/*
 * Text filter over the account list: shows the rows whose issuer or account
 * name contains filterText, case-insensitively.  Every role passes through
 * unchanged, so delegates use it exactly like accountModel.
 *
 * Deliberately NOT a QSortFilterProxyModel: on this Qt (5.6)
 * invalidateFilter() goes through beginResetModel(), and a model reset
 * rebuilds the list's header — destroying the SearchField inside it
 * mid-typing (observed as "the keyboard closes after every character",
 * one character per refocus).  This wrapper instead emits granular
 * beginRemoveRows()/beginInsertRows() for the changed span, which only
 * touches delegates.  Source-level resets (account added/edited/deleted)
 * still rebuild, but those never happen while the user is typing.
 */
class AccountFilter : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    explicit AccountFilter(QObject *parent = nullptr);

    void setSourceModel(QAbstractItemModel *source);

    QString filterText() const;
    void setFilterText(const QString &text);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void filterTextChanged();

private:
    QVector<int> mappingFor(const QString &text) const;
    void applyMapping(const QVector<int> &next);
    void rebuildFromSource();

    QAbstractItemModel *m_source;
    QString m_filterText;
    QVector<int> m_rows; // source row indices currently shown, in order
};

#endif // ACCOUNTFILTER_H
