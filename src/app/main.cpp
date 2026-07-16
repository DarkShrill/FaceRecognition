#include "FaceRecognitionController.h"
#include "FaceTypes.h"
#include "qvideostream.h"

#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include <iostream>

#include "FrameImageProvider.h"

/**
 * @brief Entry point dell'applicazione Qt Quick.
 *
 * Registra i tipi usati nelle queued connection, installa il provider immagini
 * image://frames, crea il controller C++ e lo espone a QML come faceController.
 */
int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    std::cout << "ONNX Runtime version: " << Ort::GetVersionString() << std::endl;

    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<QImage>("QImage");
    qRegisterMetaType<LandmarkMode>("LandmarkMode");
    qRegisterMetaType<FaceRecognitionOptions>("FaceRecognitionOptions");
    qRegisterMetaType<RecognitionResult>("RecognitionResult");
    QVideoStream::registerTypes();

    QQmlApplicationEngine engine;

    auto* provider = new FrameImageProvider;
    engine.addImageProvider("frames", provider);

    auto* controller = new FaceRecognitionController(provider, &app);
    QQmlEngine::setObjectOwnership(controller, QQmlEngine::CppOwnership);
    engine.rootContext()->setContextProperty("faceController", controller);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
