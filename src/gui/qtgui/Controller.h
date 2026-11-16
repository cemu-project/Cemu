#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>

class GameListModel;
class GameWindow;

// Object exposed to QML as "cemu". Glue between the QML pages and MobileBackend.
class Controller : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QAbstractItemModel* games READ games CONSTANT)
	Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
	Q_PROPERTY(bool launching READ launching NOTIFY launchingChanged)
	Q_PROPERTY(bool gameRunning READ gameRunning NOTIFY gameRunningChanged)
	Q_PROPERTY(QStringList gamePaths READ gamePaths NOTIFY gamePathsChanged)
	Q_PROPERTY(QString version READ version CONSTANT)
  public:
	explicit Controller(QObject* parent = nullptr);
	~Controller() override;

	QAbstractItemModel* games() const;
	bool scanning() const { return m_scanning; }
	bool launching() const { return m_launching; }
	bool gameRunning() const { return m_gameRunning; }
	QStringList gamePaths() const;
	QString version() const;

	Q_INVOKABLE void refresh();
	Q_INVOKABLE void launch(const QString& titleId);
	Q_INVOKABLE void addGamePath(const QUrl& folder);
	Q_INVOKABLE void addLocalGamePath(const QString& path);
	Q_INVOKABLE void removeGamePath(const QString& path);

	// stops the emulator. Must be called while the render window still exists
	void shutdown();

  Q_SIGNALS:
	void scanningChanged();
	void launchingChanged();
	void gameRunningChanged();
	void gamePathsChanged();
	void errorOccurred(const QString& message);

  private:
	void onWindowReady();
	void onWindowClosed();
	void endSession();
	void setLaunching(bool launching);

	GameListModel* m_games = nullptr;
	GameWindow* m_window = nullptr;
	QTimer m_reloadTimer;
	QTimer m_scanPollTimer;
	quint64 m_pendingTitleId = 0;
	bool m_scanning = false;
	bool m_launching = false;
	bool m_gameRunning = false;
	bool m_sessionEnded = false;
};
