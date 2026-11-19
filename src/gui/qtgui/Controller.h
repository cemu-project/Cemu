#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariant>

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

	// settings shared with the desktop interface. Keys: see SettingTable() in MobileBackend.cpp
	Q_INVOKABLE int setting(const QString& key) const;
	Q_INVOKABLE void setSetting(const QString& key, int value);
	Q_INVOKABLE QVariantList audioDevices() const; // [{id, name}], first entry = disabled
	Q_INVOKABLE QString tvAudioDevice() const;
	Q_INVOKABLE void setTvAudioDevice(const QString& id);

	// per game actions
	Q_INVOKABLE int shaderCacheCount(const QString& titleId) const;
	Q_INVOKABLE int removeShaderCaches(const QString& titleId);
	// [{index, name, path, description, enabled, presets: [{category, presets: [..], active}]}]
	Q_INVOKABLE QVariantList graphicPacks(const QString& titleId) const;
	Q_INVOKABLE void setGraphicPackEnabled(int index, bool enabled);
	Q_INVOKABLE void setGraphicPackPreset(int index, const QString& category, const QString& preset);

	// controllers. player 0 = player 1. Profiles are shared with the desktop interface
	Q_INVOKABLE QStringList inputTypes() const;
	Q_INVOKABLE QVariantMap inputPlayer(int player) const; // {enabled, type, devices: [{uuid, name, api, connected}]}
	Q_INVOKABLE QVariantList availableInputDevices() const; // [{uuid, name, api, connected}] seen by SDL right now
	Q_INVOKABLE bool setInputPlayerType(int player, const QString& type); // empty type = disabled
	Q_INVOKABLE bool assignInputDevice(int player, const QString& uuid);
	Q_INVOKABLE void removeInputDevice(int player, const QString& uuid);
	Q_INVOKABLE bool resetInputMapping(int player);
	Q_INVOKABLE QVariantList pressedInputButtons(int player) const; // [{id, name, device}]

	// stops the emulator. Must be called while the render window still exists
	void shutdown();

  Q_SIGNALS:
	void scanningChanged();
	void launchingChanged();
	void gameRunningChanged();
	void gamePathsChanged();
	void inputChanged();
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
