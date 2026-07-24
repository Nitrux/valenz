// SPDX-License-Identifier: BSD-3-Clause

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QSurfaceFormat>
#include <QProcess>
#include <QTimer>
#include <QDebug>
#include <QQmlError>
#include <QDate>
#include <QIcon>
#include <QWindow>
#include <QObject>
#include <QUrl>
#include <QMargins>
#include <QStandardPaths>
#include <QScreen>
#include <QHash>
#include <QPointer>

#include <functional>
#include <utility>

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

    Q_INVOKABLE void configurePopupWindow(QWindow *window, const QString &scope, bool keyboardOnDemand, bool wantsActiveScreen)
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
        layerShellWindow->setWantsToBeOnActiveScreen(wantsActiveScreen);
        if (!wantsActiveScreen && window->screen())
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
    }

private:
    QProcess m_proxy;
};

static void configureLayerShellWindow(QWindow *window, bool wantsActiveScreen)
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
    const int legacySpacing = qMax(0, window->property("barLayerSpacing").toInt());
    const int spacingTop = qMax(0, window->property("barLayerSpacingTop").toInt());
    const int spacingBottom = qMax(0, window->property("barLayerSpacingBottom").toInt());
    const int spacingLeft = qMax(0, window->property("barLayerSpacingLeft").toInt());
    const int spacingRight = qMax(0, window->property("barLayerSpacingRight").toInt());
    const bool hasDirectionalSpacing = spacingTop > 0 || spacingBottom > 0 || spacingLeft > 0 || spacingRight > 0;
    const int appliedTop = hasDirectionalSpacing ? spacingTop : legacySpacing;
    const int appliedBottom = hasDirectionalSpacing ? spacingBottom : 0;
    const int appliedLeft = hasDirectionalSpacing ? spacingLeft : 0;
    const int appliedRight = hasDirectionalSpacing ? spacingRight : 0;
    const int exclusiveZone = barHeight + appliedTop + appliedBottom;
    layerShellWindow->setExclusiveZone(window->isVisible()
                                           ? (exclusiveZone > 0 ? exclusiveZone : window->height())
                                           : 0);
    layerShellWindow->setWantsToBeOnActiveScreen(wantsActiveScreen);
    if (!wantsActiveScreen && window->screen())
        layerShellWindow->setScreen(window->screen());

    LayerShellQt::Window::Anchors anchors;
    anchors |= LayerShellQt::Window::AnchorTop;
    anchors |= LayerShellQt::Window::AnchorLeft;
    anchors |= LayerShellQt::Window::AnchorRight;
    layerShellWindow->setAnchors(anchors);
    layerShellWindow->setDesiredSize(QSize(window->width(), window->height()));
    layerShellWindow->setMargins(QMargins(appliedLeft, appliedTop, appliedRight, appliedBottom));
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
    QQmlComponent component(&engine, url);
    if (component.isError()) {
        for (const QQmlError &error : component.errors())
            qWarning().noquote() << error.toString();
        return -1;
    }

    bool allScreensMode = false;
    QHash<QScreen *, QPointer<QWindow>> screenWindows;
    QPointer<QWindow> activeScreenWindow;

    const auto scheduleWindowConfigure = [&](QWindow *window) {
        const QPointer<QWindow> guardedWindow(window);
        QTimer::singleShot(0, &app, [guardedWindow, &allScreensMode] {
            if (guardedWindow)
                configureLayerShellWindow(guardedWindow, !allScreensMode);
        });
    };

    std::function<QWindow *(QScreen *)> createBarWindow;
    createBarWindow = [&](QScreen *screen) -> QWindow * {
        if (allScreensMode && screen && screenWindows.value(screen))
            return screenWindows.value(screen);
        if (!allScreensMode && activeScreenWindow)
            return activeScreenWindow;

        QObject *object = component.create(engine.rootContext());
        if (!object) {
            for (const QQmlError &error : component.errors())
                qWarning().noquote() << error.toString();
            return nullptr;
        }

        auto *window = qobject_cast<QWindow *>(object);
        if (!window) {
            object->deleteLater();
            return nullptr;
        }

        if (screen)
            window->setScreen(screen);

        QObject::connect(window, &QWindow::widthChanged, window,
                         [&, window](int) { scheduleWindowConfigure(window); });
        QObject::connect(window, &QWindow::heightChanged, window,
                         [&, window](int) { scheduleWindowConfigure(window); });
        QObject::connect(window, &QWindow::visibleChanged, window,
                         [&, window](bool) { scheduleWindowConfigure(window); });

        window->close();
        configureLayerShellWindow(window, !allScreensMode);
        window->show();

        if (allScreensMode && screen) {
            screenWindows.insert(screen, window);
            QObject::connect(window, &QObject::destroyed, &app, [&, screen, window] {
                if (screenWindows.value(screen).data() == window)
                    screenWindows.remove(screen);
            });
        } else {
            activeScreenWindow = window;
            QObject::connect(window, &QObject::destroyed, &app, [&, window] {
                if (activeScreenWindow.data() == window)
                    activeScreenWindow.clear();
            });
        }

        return window;
    };

    const auto reconcileBarWindows = [&] {
        const bool wantsAllScreens = valenzBridge.screenPlacement() == QLatin1String("all");
        if (wantsAllScreens) {
            const QPointer<QWindow> previousActiveWindow = activeScreenWindow;
            allScreensMode = true;
            for (QScreen *screen : app.screens())
                createBarWindow(screen);

            if (!screenWindows.isEmpty()) {
                activeScreenWindow.clear();
                if (previousActiveWindow) {
                    previousActiveWindow->close();
                    previousActiveWindow->deleteLater();
                }
            } else {
                allScreensMode = false;
                activeScreenWindow = previousActiveWindow;
                if (activeScreenWindow)
                    scheduleWindowConfigure(activeScreenWindow);
            }
            return;
        }

        const auto previousScreenWindows = screenWindows;
        screenWindows.clear();
        allScreensMode = false;
        createBarWindow(nullptr);
        if (!activeScreenWindow) {
            screenWindows = previousScreenWindows;
            allScreensMode = true;
            for (const QPointer<QWindow> &window : std::as_const(screenWindows)) {
                if (window)
                    scheduleWindowConfigure(window);
            }
            return;
        }

        for (const QPointer<QWindow> &window : previousScreenWindows) {
            if (window) {
                window->close();
                window->deleteLater();
            }
        }
    };


    const auto reconfigureBarWindows = [&] {
        if (activeScreenWindow)
            scheduleWindowConfigure(activeScreenWindow);
        for (const QPointer<QWindow> &window : std::as_const(screenWindows)) {
            if (window)
                scheduleWindowConfigure(window);
        }
    };

    QObject::connect(&valenzBridge, &ValenzBridge::screenPlacementChanged, &app,
                     [&](const QString &) { reconcileBarWindows(); });
    QObject::connect(&valenzBridge, &ValenzBridge::barHeightChanged, &app,
                     [&](int) { reconfigureBarWindows(); });
    QObject::connect(&valenzBridge, &ValenzBridge::barLayerSpacingChanged, &app,
                     [&](int) { reconfigureBarWindows(); });
    QObject::connect(&valenzBridge, &ValenzBridge::barLayerSpacingTopChanged, &app,
                     [&](int) { reconfigureBarWindows(); });
    QObject::connect(&valenzBridge, &ValenzBridge::barLayerSpacingBottomChanged, &app,
                     [&](int) { reconfigureBarWindows(); });
    QObject::connect(&valenzBridge, &ValenzBridge::barLayerSpacingLeftChanged, &app,
                     [&](int) { reconfigureBarWindows(); });
    QObject::connect(&valenzBridge, &ValenzBridge::barLayerSpacingRightChanged, &app,
                     [&](int) { reconfigureBarWindows(); });

    QObject::connect(&app, &QGuiApplication::screenAdded, &app,
                     [&](QScreen *screen) {
                         if (allScreensMode)
                             createBarWindow(screen);
                     });
    QObject::connect(&app, &QGuiApplication::screenRemoved, &app,
                     [&](QScreen *screen) {
                         const QPointer<QWindow> window = screenWindows.take(screen);
                         if (window) {
                             window->close();
                             window->deleteLater();
                         }
                     });

    reconcileBarWindows();

    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &systemTrayController,
                     [&systemTrayController](Qt::ApplicationState state) {
                         if (state == Qt::ApplicationActive)
                             QTimer::singleShot(0, &systemTrayController, &SystemTrayController::refresh);
                     });

    return app.exec();
}

#include "main.moc"
