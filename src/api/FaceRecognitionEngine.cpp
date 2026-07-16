#include "FaceRecognitionEngine.h"

#include "FaceRecognitionWorker.h"

#include <QMetaObject>

FaceRecognitionEngine::FaceRecognitionEngine(InferenceDevice device, QObject* parent)
    : QObject(parent),
      m_worker(new FaceRecognitionWorker(device)) {
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<LandmarkMode>("LandmarkMode");
    qRegisterMetaType<FaceRecognitionOptions>("FaceRecognitionOptions");
    qRegisterMetaType<RecognitionResult>("RecognitionResult");

    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &FaceRecognitionWorker::busyChanged, this, [this](bool busy) {
        if (m_busy == busy) {
            return;
        }
        m_busy = busy;
        emit busyChanged(m_busy);
    });
    connect(m_worker, &FaceRecognitionWorker::resultReady, this, [this](const RecognitionResult& result) {
        emit resultReady(result);
        if (m_options.landmarkMode != LandmarkMode::None) {
            emit landmarksReady(result);
        }
    });
    m_workerThread.start();
}

FaceRecognitionEngine::~FaceRecognitionEngine() {
    m_workerThread.quit();
    m_workerThread.wait();
}

bool FaceRecognitionEngine::initialize(const QString& detectorModel,
                                       const QString& landmarkModel,
                                       const QString& landmark3dModel,
                                       const QString& recognizerModel,
                                       const QString& embeddingsDir) {
    bool initOk = false;
    QMetaObject::invokeMethod(m_worker,
                              [&]() {
                                  m_worker->setOptions(m_options);
                                  initOk = m_worker->initialize(detectorModel,
                                                               landmarkModel,
                                                               landmark3dModel,
                                                               recognizerModel,
                                                               embeddingsDir);
                              },
                              Qt::BlockingQueuedConnection);

    if (m_initialized != initOk) {
        m_initialized = initOk;
        emit initializedChanged();
    }
    return initOk;
}

bool FaceRecognitionEngine::reloadEmbeddings(const QString& embeddingsDir) {
    if (!m_initialized) {
        return false;
    }

    bool loadOk = false;
    QMetaObject::invokeMethod(m_worker,
                              [&]() {
                                  loadOk = m_worker->reloadEmbeddings(embeddingsDir);
                              },
                              Qt::BlockingQueuedConnection);
    return loadOk;
}

int FaceRecognitionEngine::landmarkMode() const {
    return static_cast<int>(m_options.landmarkMode);
}

bool FaceRecognitionEngine::isInitialized() const {
    return m_initialized;
}

bool FaceRecognitionEngine::isBusy() const {
    return m_busy;
}

void FaceRecognitionEngine::setLandmarkMode(int mode) {
    const LandmarkMode sanitized = sanitizeLandmarkMode(mode);
    if (m_options.landmarkMode == sanitized) {
        return;
    }

    m_options.landmarkMode = sanitized;
    applyOptions();
    emit optionsChanged();
}

void FaceRecognitionEngine::submitFrame(cv::Mat frame) {
    if (!m_initialized || frame.empty() || m_busy) {
        return;
    }

    cv::Mat copy = frame.clone();
    emit frameSubmitted(copy);
    QMetaObject::invokeMethod(m_worker, "processFrame", Qt::QueuedConnection, Q_ARG(cv::Mat, copy));
}

void FaceRecognitionEngine::applyOptions() {
    QMetaObject::invokeMethod(m_worker,
                              "setOptions",
                              Qt::QueuedConnection,
                              Q_ARG(FaceRecognitionOptions, m_options));
}

LandmarkMode FaceRecognitionEngine::sanitizeLandmarkMode(int mode) {
    switch (static_cast<LandmarkMode>(mode)) {
    case LandmarkMode::None:
    case LandmarkMode::FivePoints:
    case LandmarkMode::Points106:
    case LandmarkMode::All:
    case LandmarkMode::Points3D68:
        return static_cast<LandmarkMode>(mode);
    }
    return LandmarkMode::All;
}
