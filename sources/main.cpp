//*******************************************************************************
// ZX Tape Reviver
//-----------------
//
// Author: Leonid Golouz
// E-mail: lgolouz@list.ru
// YouTube channel: https://www.youtube.com/channel/UCz_ktTqWVekT0P4zVW8Xgcg
// YouTube channel e-mail: computerenthusiasttips@mail.ru
//
// Code modification and distribution of any kind is not allowed without direct
// permission of the Author.
//*******************************************************************************

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "sources/controls/waveformcontrol.h"
#include "sources/core/waveformparser.h"
#include "sources/models/fileworkermodel.h"
#include "sources/models/suspiciouspointsmodel.h"
#include "sources/models/parsersettingsmodel.h"

void registerTypes()
{
    qmlRegisterUncreatableType<FileWorkerModel>("com.enums.zxtapereviver", 1, 0, "FileWorkerResults", QString());
    qmlRegisterUncreatableType<WavReader>("com.enums.zxtapereviver", 1, 0, "ErrorCodesEnum", QString());
    qmlRegisterUncreatableType<WaveformControl>("com.enums.zxtapereviver", 1, 0, "WaveformControlOperationModes", QString());

    qmlRegisterType<WaveformControl>("WaveformControl", 1, 0, "WaveformControl");
    qmlRegisterSingletonType<FileWorkerModel>("com.models.zxtapereviver", 1, 0, "FileWorkerModel", [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject* {
        Q_UNUSED(engine)
        Q_UNUSED(scriptEngine)
        return new FileWorkerModel();
    });
    qmlRegisterSingletonInstance<SuspiciousPointsModel>("com.models.zxtapereviver", 1, 0, "SuspiciousPointsModel", SuspiciousPointsModel::instance());
    qmlRegisterSingletonInstance<WavReader>("com.core.zxtapereviver", 1, 0, "WavReader", WavReader::instance());
    qmlRegisterSingletonInstance<WaveformParser>("com.core.zxtapereviver", 1, 0, "WaveformParser", WaveformParser::instance());
    qmlRegisterSingletonInstance<ParserSettingsModel>("com.models.zxtapereviver", 1, 0, "ParserSettingsModel", ParserSettingsModel::instance());
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    registerTypes();

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
