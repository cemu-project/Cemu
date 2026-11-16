#include "Controller.h"
#include "GameListModel.h"
#include "GameWindow.h"
#include "MobileBackend.h"

#include <QCoreApplication>
#include <QDir>

#include <unistd.h>

Controller::Controller(QObject* parent) : QObject(parent)
{
	MobileBackend::Initialize();

	m_games = new GameListModel(this);

	// title list events arrive in bursts (one per discovered title) and from other threads: coalesce them
	m_reloadTimer.setSingleShot(true);
	m_reloadTimer.setInterval(150);
	connect(&m_reloadTimer, &QTimer::timeout, this, [this] { m_games->reload(); });

	MobileBackend::SetGameListChangedCallback([this] {
		QMetaObject::invokeMethod(this, [this] { m_reloadTimer.start(); }, Qt::QueuedConnection);
	});
	MobileBackend::SetGameExitedCallback([this] {
		QMetaObject::invokeMethod(this, [this] { endSession(); }, Qt::QueuedConnection);
	});

	m_scanPollTimer.setInterval(400);
	connect(&m_scanPollTimer, &QTimer::timeout, this, [this] {
		const bool scanning = MobileBackend::IsScanning();
		if (scanning == m_scanning)
			return;
		m_scanning = scanning;
		Q_EMIT scanningChanged();
		if (!scanning)
			m_games->reload();
	});
	m_scanPollTimer.start();

	m_scanning = MobileBackend::IsScanning();
	m_games->reload();
}

Controller::~Controller()
{
	MobileBackend::SetGameListChangedCallback(nullptr);
	MobileBackend::SetGameExitedCallback(nullptr);
}

QAbstractItemModel* Controller::games() const
{
	return m_games;
}

QStringList Controller::gamePaths() const
{
	QStringList result;
	for (const std::string& p : MobileBackend::GetGamePaths())
		result.push_back(QString::fromStdString(p));
	return result;
}

QString Controller::version() const
{
	return QString::fromStdString(MobileBackend::GetVersionString());
}

void Controller::refresh()
{
	MobileBackend::RefreshGameList();
}

void Controller::addGamePath(const QUrl& folder)
{
	addLocalGamePath(folder.isLocalFile() ? folder.toLocalFile() : folder.toString());
}

void Controller::addLocalGamePath(const QString& rawPath)
{
	QString path = rawPath.trimmed();
	if (path.startsWith(QLatin1String("~/")))
		path = QDir::homePath() + path.mid(1);
	if (path.isEmpty())
		return;
	path = QDir::cleanPath(path);
	MobileBackend::AddGamePath(path.toStdString());
	Q_EMIT gamePathsChanged();
}

void Controller::removeGamePath(const QString& path)
{
	MobileBackend::RemoveGamePath(path.toStdString());
	Q_EMIT gamePathsChanged();
}

void Controller::setLaunching(bool launching)
{
	if (m_launching == launching)
		return;
	m_launching = launching;
	Q_EMIT launchingChanged();
}

void Controller::launch(const QString& titleId)
{
	if (m_window || m_gameRunning || m_launching)
		return;
	bool ok = false;
	m_pendingTitleId = titleId.toULongLong(&ok, 16);
	if (!ok)
		return;
	setLaunching(true);

	m_window = new GameWindow();
	// queued: creating the swapchain from inside the expose handler would re-enter the platform plugin
	connect(m_window, &GameWindow::ready, this, &Controller::onWindowReady, Qt::QueuedConnection);
	connect(m_window, &GameWindow::closed, this, &Controller::onWindowClosed);
	m_window->showFullScreen();
}

void Controller::onWindowReady()
{
	if (!m_window || m_gameRunning)
		return;
	std::string error;
	const MobileBackend::NativeWindow nativeWindow = m_window->nativeWindow();
	if (!MobileBackend::LaunchGame(m_pendingTitleId, nativeWindow, error))
	{
		GameWindow* window = m_window;
		m_window = nullptr;
		window->hide();
		window->deleteLater();
		setLaunching(false);
		Q_EMIT errorOccurred(QString::fromStdString(error));
		return;
	}
	m_gameRunning = true;
	Q_EMIT gameRunningChanged();
	setLaunching(false);
	m_window->requestActivate();
}

void Controller::onWindowClosed()
{
	if (m_gameRunning)
	{
		endSession();
		return;
	}
	// closed before the game started
	if (m_window)
	{
		m_window->deleteLater();
		m_window = nullptr;
	}
	setLaunching(false);
}

void Controller::endSession()
{
	if (m_sessionEnded)
		return;
	m_sessionEnded = true;
	// Stopping a title and starting another one inside the same process is not reliable in the emulator core
	// (the desktop UI has the same limitation). Shut down cleanly and start over with a fresh process instead,
	// which brings the user straight back to the game list.
	shutdown();
	MobileBackend::RestartProcess();
	// Only reached if the restart failed. Leave without running static destructors: the Vulkan swapchain
	// still references this window's surface and tearing both down in arbitrary order can crash.
	::_exit(0);
}

void Controller::shutdown()
{
	m_scanPollTimer.stop();
	m_reloadTimer.stop();
	MobileBackend::Shutdown();
}
