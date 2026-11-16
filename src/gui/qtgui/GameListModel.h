#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class GameListModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(int count READ count NOTIFY countChanged)
  public:
	enum Roles
	{
		TitleIdRole = Qt::UserRole + 1,
		NameRole,
		RegionRole,
		VersionRole,
		HasUpdateRole,
		HasDLCRole,
		IconRole,
	};

	explicit GameListModel(QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	int count() const { return m_entries.size(); }

	// re-reads the title list from the emulator core
	void reload();

  Q_SIGNALS:
	void countChanged();

  private:
	struct Entry
	{
		QString titleId; // 16 hex digits
		QString name;
		QString region;
		int version = 0;
		bool hasUpdate = false;
		bool hasDLC = false;
	};
	QVector<Entry> m_entries;
};
