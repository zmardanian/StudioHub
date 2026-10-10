#include "projectcontroller.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#include <QTimer>
#include <QDebug>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("StudioHub Desktop");
    QCoreApplication::setApplicationVersion(STUDIOHUB_DESKTOP_VERSION);

    ProjectController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("studio", &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    if(app.arguments().contains("--smoke-test")) {
        qInfo()<<"StudioHub QML initialized";
        QTimer::singleShot(1200,&app,&QCoreApplication::quit);
    }
    return app.exec();
}
