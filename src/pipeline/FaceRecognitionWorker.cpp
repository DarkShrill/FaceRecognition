#include "FaceRecognitionWorker.h"

FaceRecognitionWorker::FaceRecognitionWorker(InferenceDevice device, QObject* parent)
    : QObject(parent), m_pipeline(device) {}

bool FaceRecognitionWorker::initialize(const QString& detectorModel,
                                       const QString& landmarkModel,
                                       const QString& landmark3dModel,
                                       const QString& recognizerModel,
                                       const QString& embeddingsDir) {
    return m_pipeline.initialize(detectorModel, landmarkModel, landmark3dModel, recognizerModel, embeddingsDir);
}

bool FaceRecognitionWorker::reloadEmbeddings(const QString& embeddingsDir) {
    return m_pipeline.reloadEmbeddings(embeddingsDir);
}

void FaceRecognitionWorker::setOptions(FaceRecognitionOptions options) {
    m_pipeline.setOptions(options);
}

void FaceRecognitionWorker::processFrame(cv::Mat frame) {
    if (m_busy.exchange(true)) {
        return;
    }
    emit busyChanged(true);

    const RecognitionResult result = m_pipeline.process(frame);
    emit resultReady(result);

    m_busy.store(false);
    emit busyChanged(false);
}
