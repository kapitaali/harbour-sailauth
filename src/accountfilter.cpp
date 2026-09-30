#include "accountfilter.h"
#include "accountmodel.h"

AccountFilter::AccountFilter(QObject *parent)
    : QAbstractListModel(parent)
    , m_source(nullptr)
{
}

void AccountFilter::setSourceModel(QAbstractItemModel *source)
{
    if (m_source == source)
        return;
    if (m_source)
        disconnect(m_source, nullptr, this, nullptr);

    m_source = source;
    if (!m_source)
        return;

    // Relay only the visible rows; the once-per-second countdown tick comes
    // through here as dataChanged(code, remaining).
    connect(m_source, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &top, const QModelIndex &bottom,
                   const QVector<int> &roles) {
        for (int row = 0; row < m_rows.size(); ++row) {
            const int src = m_rows.at(row);
            if (src >= top.row() && src <= bottom.row())
                emit dataChanged(index(row), index(row), roles);
        }
    });

    // Account mutations reset the source; those are navigation-time events,
    // never typing-time, so a rebuild (own reset) is acceptable there.
    connect(m_source, &QAbstractItemModel::modelReset,
            this, &AccountFilter::rebuildFromSource);
    connect(m_source, &QAbstractItemModel::rowsInserted,
            this, &AccountFilter::rebuildFromSource);
    connect(m_source, &QAbstractItemModel::rowsRemoved,
            this, &AccountFilter::rebuildFromSource);

    rebuildFromSource();
}

QString AccountFilter::filterText() const
{
    return m_filterText;
}

void AccountFilter::setFilterText(const QString &text)
{
    if (m_filterText == text)
        return;
    m_filterText = text;
    emit filterTextChanged();
    applyMapping(mappingFor(text));
}

int AccountFilter::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant AccountFilter::data(const QModelIndex &index, int role) const
{
    if (!m_source || !index.isValid()
            || index.row() < 0 || index.row() >= m_rows.size())
        return QVariant();
    return m_source->data(m_source->index(m_rows.at(index.row()), 0), role);
}

QHash<int, QByteArray> AccountFilter::roleNames() const
{
    return m_source ? m_source->roleNames() : QHash<int, QByteArray>();
}

QVector<int> AccountFilter::mappingFor(const QString &text) const
{
    QVector<int> rows;
    if (!m_source)
        return rows;

    const int sourceCount = m_source->rowCount();
    for (int r = 0; r < sourceCount; ++r) {
        if (text.isEmpty()) {
            rows.append(r);
            continue;
        }
        const QModelIndex idx = m_source->index(r, 0);
        const QString issuer = m_source->data(idx, AccountModel::IssuerRole).toString();
        const QString name = m_source->data(idx, AccountModel::NameRole).toString();
        if (issuer.contains(text, Qt::CaseInsensitive)
                || name.contains(text, Qt::CaseInsensitive)) {
            rows.append(r);
        }
    }
    return rows;
}

/*
 * Move from m_rows to `next` by removing and inserting only the span where
 * they differ: both sequences are ordered source row indices, so the shared
 * prefix and shared suffix bracket the change.  Never a reset — that is the
 * whole point (see the header comment).
 */
void AccountFilter::applyMapping(const QVector<int> &next)
{
    if (next == m_rows)
        return;

    const int oldCount = m_rows.size();
    const int newCount = next.size();

    int first = 0;
    while (first < oldCount && first < newCount && m_rows.at(first) == next.at(first))
        ++first;

    int lastOld = oldCount - 1;
    int lastNew = newCount - 1;
    while (lastOld >= first && lastNew >= first
           && m_rows.at(lastOld) == next.at(lastNew)) {
        --lastOld;
        --lastNew;
    }

    const int removeCount = lastOld - first + 1;
    const int insertCount = lastNew - first + 1;

    if (removeCount > 0) {
        beginRemoveRows(QModelIndex(), first, first + removeCount - 1);
        m_rows.remove(first, removeCount);
        endRemoveRows();
    }
    if (insertCount > 0) {
        beginInsertRows(QModelIndex(), first, first + insertCount - 1);
        for (int k = 0; k < insertCount; ++k)
            m_rows.insert(first + k, next.at(first + k));
        endInsertRows();
    }
}

void AccountFilter::rebuildFromSource()
{
    beginResetModel();
    m_rows = mappingFor(m_filterText);
    endResetModel();
}
