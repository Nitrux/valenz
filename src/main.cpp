// SPDX-License-Identifier: BSD-3-Clause

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSurfaceFormat>
#include <QUrl>

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

    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Maui"));

    KLocalizedString::setApplicationDomain("valenz");

    // Ensure MauiKit core is initialized so Maui QML resources are available.
    MauiApp::instance();

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
