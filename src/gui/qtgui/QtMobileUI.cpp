#include "QtMobileUI.h"
#include "Controller.h"
#include "MobileBackend.h"

#include <QApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>

static void InitMobileResources()
{
	// the resources live in a static library, so they have to be registered explicitly
	Q_INIT_RESOURCE(cemu_mobile);
}

namespace
{
	// serves "image://gameicon/<titleId hex>". Runs on Qt's image loader thread
	class GameIconProvider : public QQuickImageProvider
	{
	  public:
		GameIconProvider() : QQuickImageProvider(QQuickImageProvider::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
		{
		}

		QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override
		{
			bool ok = false;
			const quint64 titleId = id.toULongLong(&ok, 16);
			std::vector<uint8_t> rgba;
			int w = 0, h = 0;
			if (!ok || !MobileBackend::LoadGameIcon(titleId, rgba, w, h) || w <= 0 || h <= 0)
				return {};
			QImage image = QImage(rgba.data(), w, h, w * 4, QImage::Format_RGBA8888).copy();
			if (size)
				*size = image.size();
			if (requestedSize.isValid() && requestedSize != image.size())
				image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			return image;
		}
	};
} // namespace

int QtMobileUI::Run()
{
	InitMobileResources();

	static int argc = 1;
	static char arg0[] = "Cemu";
	static char* argv[] = {arg0, nullptr};

	QCoreApplication::setApplicationName(QStringLiteral("Cemu"));
	QGuiApplication::setDesktopFileName(QStringLiteral("info.cemu.Cemu"));
	// QApplication (not QGuiApplication) so that the Plasma "org.kde.desktop" style, which is widget based, works
	QApplication app(argc, argv);

	Controller controller;

	int result = 0;
	{
		QQmlApplicationEngine engine;
		engine.addImageProvider(QStringLiteral("gameicon"), new GameIconProvider);
		engine.rootContext()->setContextProperty(QStringLiteral("cemu"), &controller);
		engine.load(QUrl(QStringLiteral("qrc:/cemu/mobile/Main.qml")));
		if (engine.rootObjects().isEmpty())
		{
			controller.shutdown();
			return 1;
		}
		result = app.exec();
		// stop the emulator while the render window and the UI still exist
		controller.shutdown();
	}
	return result;
}
