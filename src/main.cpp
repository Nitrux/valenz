// SPDX-License-Identifier: BSD-3-Clause

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSurfaceFormat>
#include <QProcess>
#include <QDate>
#include <QIcon>
#include <QWindow>
#include <QObject>
#include <QUrl>
#include <QMargins>
#include <QStandardPaths>
#include <QScreen>

#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>

#include <KAboutData>
#include <KLocalizedContext>
#include <KLocalizedString>

#include <MauiKit4/Core/mauiapp.h>
#include "controllers/valenzbridge_notifications.h"
#include "controllers/valenzbridge_systray.h"
#include "controllers/valenzbridge.h"

class LayerShellPopupHelper : public QObject
{
    Q_OBJECT

public:
    explicit LayerShellPopupHelper(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE void configurePopupWindow(QWindow *window, const QString &scope, bool keyboardOnDemand)
    {
        if (!window)
            return;

        auto *layerShellWindow = LayerShellQt::Window::get(window);
        if (!layerShellWindow)
            return;

        layerShellWindow->setScope(scope);
        layerShellWindow->setLayer(LayerShellQt::Window::LayerTop);
        layerShellWindow->setKeyboardInteractivity(keyboardOnDemand
                                                        ? LayerShellQt::Window::KeyboardInteractivityOnDemand
                                                        : LayerShellQt::Window::KeyboardInteractivityNone);
        layerShellWindow->setWantsToBeOnActiveScreen(true);
        layerShellWindow->setScreen(window->screen());

        LayerShellQt::Window::Anchors anchors{};
        layerShellWindow->setAnchors(anchors);
        layerShellWindow->setExclusiveZone(0);
        layerShellWindow->setDesiredSize(QSize(window->width(), window->height()));
        layerShellWindow->setMargins(QMargins(0, 0, 0, 0));
    }
};

class LegacyTrayProxyHelper : public QObject
{
    Q_OBJECT

public:
    explicit LegacyTrayProxyHelper(QObject *parent = nullptr) : QObject(parent) {}

    void start()
    {
        if (!QGuiApplication::platformName().contains(QStringLiteral("wayland"), Qt::CaseInsensitive))
            return;

        const QString proxyExecutable = QStandardPaths::findExecutable(QStringLiteral("xembedsniproxy"));
        if (proxyExecutable.isEmpty())
            return;

        if (m_proxy.state() != QProcess::NotRunning)
            return;

        m_proxy.start(proxyExecutable);
        if (!m_proxy.waitForStarted(3000))
            m_proxy.close();
    }

private:
    QProcess m_proxy;
};

static void configureLayerShellWindow(QWindow *window)
{
    if (!window)
        return;

    auto *layerShellWindow = LayerShellQt::Window::get(window);
    if (!layerShellWindow)
        return;

    layerShellWindow->setScope(QStringLiteral("org.maui.valenz"));
    layerShellWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    layerShellWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    const int barHeight = window->property("barHeight").toInt();
    const int barLayerSpacing = qMax(0, window->property("barLayerSpacing").toInt());
    const int exclusiveZone = barHeight + barLayerSpacing;
    layerShellWindow->setExclusiveZone(exclusiveZone > 0 ? exclusiveZone : window->height());
    layerShellWindow->setWantsToBeOnActiveScreen(true);
    layerShellWindow->setScreen(window->screen());

    LayerShellQt::Window::Anchors anchors;
    anchors |= LayerShellQt::Window::AnchorTop;
    layerShellWindow->setAnchors(anchors);
    layerShellWindow->setDesiredSize(QSize(window->width(), window->height()));
    layerShellWindow->setMargins(QMargins(0, barLayerSpacing, 0, 0));
}

static QString desktopFileNameForPortal()
{
    return QStringLiteral("org.maui.valenz");
}

int main(int argc, char *argv[])
{
    KLocalizedString::setApplicationDomain("valenz");

    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    KAboutData aboutData(QStringLiteral("valenz"),
                         i18n("Valenz"),
                         QStringLiteral("0.1.0"),
                         i18n("Status bar for Nitrux built with MauiKit."),
                         KAboutLicense::BSD_3_Clause,
                         i18n("© %1 Made by Nitrux | Built with MauiKit", QString::number(QDate::currentDate().year())),
                         QStringLiteral("main/92be48c"));
    aboutData.setHomepage(QStringLiteral("https://github.com/Nitrux/valenz"));
    aboutData.setBugAddress(QByteArrayLiteral("https://github.com/Nitrux/valenz/issues"));
    aboutData.addAuthor(QStringLiteral("Uri Herrera"), i18n("Developer"), QStringLiteral("uri_herrera@nxos.org"));
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("valenz"));
    app.setApplicationDisplayName(QStringLiteral("Valenz"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows")));
    app.setOrganizationName(QStringLiteral("Maui"));

    const QString desktopFileName = desktopFileNameForPortal();
    if (!desktopFileName.isEmpty())
        app.setDesktopFileName(desktopFileName);

    aboutData.setProductName(QByteArrayLiteral("nitrux/valenz"));
    aboutData.setOrganizationDomain(QByteArrayLiteral("org.maui.valenz"));
    if (!desktopFileName.isEmpty())
        aboutData.setDesktopFileName(desktopFileName.toUtf8());
    aboutData.setProgramLogo(app.windowIcon());
    KAboutData::setApplicationData(aboutData);

    // Ensure MauiKit core is initialized so Maui QML resources are available.
    MauiApp::instance();
    // TODO: Replace this themed icon with a Valenz-specific asset later.
    MauiApp::instance()->setIconName(QStringLiteral("preferences-system-windows"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    LayerShellPopupHelper layerShellPopupHelper;
    engine.rootContext()->setContextProperty(QStringLiteral("layerShellHelper"), &layerShellPopupHelper);
    LegacyTrayProxyHelper legacyTrayProxyHelper;
    legacyTrayProxyHelper.start();
    ValenzBridge valenzBridge;
    SystemTrayController systemTrayController;
    NotificationsController notificationsController;
    engine.rootContext()->setContextProperty(QStringLiteral("valenzBridge"), &valenzBridge);
    engine.rootContext()->setContextProperty(QStringLiteral("systemTrayController"), &systemTrayController);
    engine.rootContext()->setContextProperty(QStringLiteral("notificationsController"), &notificationsController);

    const QUrl url(QStringLiteral("qrc:/app/valenz/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [url](QObject *obj, const QUrl &objUrl)
    {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
        if (obj && url == objUrl) {
            if (auto *window = qobject_cast<QWindow *>(obj)) {
                window->close();
                configureLayerShellWindow(window);
                window->show();
            }
        }
    });

    engine.load(url);

    return app.exec();
}

#include "main.moc"
