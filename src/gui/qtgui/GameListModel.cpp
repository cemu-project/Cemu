#include "GameListModel.h"
#include "MobileBackend.h"

GameListModel::GameListModel(QObject* parent) : QAbstractListModel(parent)
{
}

int GameListModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : m_entries.size();
}

QVariant GameListModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
		return {};
	const Entry& e = m_entries.at(index.row());
	switch (role)
	{
	case Qt::DisplayRole:
	case NameRole:
		return e.name;
	case TitleIdRole:
		return e.titleId;
	case RegionRole:
		return e.region;
	case VersionRole:
		return e.version;
	case HasUpdateRole:
		return e.hasUpdate;
	case HasDLCRole:
		return e.hasDLC;
	case IconRole:
		return QStringLiteral("image://gameicon/") + e.titleId;
	default:
		return {};
	}
}

QHash<int, QByteArray> GameListModel::roleNames() const
{
	return {
		{TitleIdRole, "titleId"},
		{NameRole, "name"},
		{RegionRole, "region"},
		{VersionRole, "version"},
		{HasUpdateRole, "hasUpdate"},
		{HasDLCRole, "hasDLC"},
		{IconRole, "iconSource"},
	};
}

void GameListModel::reload()
{
	const std::vector<MobileBackend::GameEntry> games = MobileBackend::GetGames();
	QVector<Entry> entries;
	entries.reserve(static_cast<int>(games.size()));
	for (const auto& g : games)
	{
		Entry e;
		e.titleId = QStringLiteral("%1").arg(static_cast<qulonglong>(g.titleId), 16, 16, QLatin1Char('0'));
		e.name = QString::fromStdString(g.name);
		e.region = QString::fromStdString(g.region);
		e.version = g.version;
		e.hasUpdate = g.hasUpdate;
		e.hasDLC = g.hasDLC;
		entries.push_back(e);
	}

	// skip the reset (and the flicker/scroll jump it causes) when nothing changed
	bool same = entries.size() == m_entries.size();
	for (int i = 0; same && i < entries.size(); i++)
	{
		const Entry& a = entries.at(i);
		const Entry& b = m_entries.at(i);
		same = a.titleId == b.titleId && a.name == b.name && a.region == b.region && a.version == b.version &&
			   a.hasUpdate == b.hasUpdate && a.hasDLC == b.hasDLC;
	}
	if (same)
		return;

	const int oldCount = m_entries.size();
	beginResetModel();
	m_entries = entries;
	endResetModel();
	if (oldCount != m_entries.size())
		Q_EMIT countChanged();
}
