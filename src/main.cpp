// SPDX-License-Identifier: BSD-3-Clause

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSurfaceFormat>
#include <QDate>
#include <QIcon>
#include <QUrl>

#include <KAboutData>
#include <KLocalizedContext>
#include <KLocalizedString>

#include <MauiKit4/Core/mauiapp.h>

#include "controllers/valenzbridge_notifications.h"
#include "controllers/valenzbridge_systray.h"
#include "controllers/valenzbridge.h"

int main(int argc, char *argv[])
{
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

    aboutData.setProductName(QByteArrayLiteral("nitrux/valenz"));
    aboutData.setOrganizationDomain(QByteArrayLiteral("org.maui.valenz"));
    aboutData.setDesktopFileName(QByteArrayLiteral("org.maui.valenz"));
    aboutData.setProgramLogo(app.windowIcon());
    KAboutData::setApplicationData(aboutData);

    KLocalizedString::setApplicationDomain("valenz");

    // Ensure MauiKit core is initialized so Maui QML resources are available.
    MauiApp::instance();
    // TODO: Replace this placeholder with a Valenz-specific app icon.
    MauiApp::instance()->setIconName(QStringLiteral("preferences-system-windows"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
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
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
