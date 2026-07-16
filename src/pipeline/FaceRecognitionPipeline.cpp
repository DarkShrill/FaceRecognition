#include "FaceRecognitionPipeline.h"

#include <chrono>

FaceRecognitionPipeline::FaceRecognitionPipeline(InferenceDevice device)
    : m_detector(device), m_landmarker(device), m_landmarker3d68(device), m_recognizer(device) {}

bool FaceRecognitionPipeline::initialize(const QString& detectorModel,
                                         const QString& landmarkModel,
                                         const QString& landmark3dModel,
                                         const QString& recognizerModel,
                                         const QString& embeddingsDir) {
    const bool detectorReady = m_detector.load(detectorModel, cv::Size(640, 640));
    m_landmarkerReady = !landmarkModel.isEmpty() && m_landmarker.load(landmarkModel);
    m_landmarker3dReady = !landmark3dModel.isEmpty() && m_landmarker3d68.load(landmark3dModel);
    const bool recognizerReady = m_recognizer.load(recognizerModel);
    m_database.load(embeddingsDir);

    return detectorReady
        && recognizerReady
        && (!shouldExpose106Landmarks(m_options.landmarkMode) || m_landmarkerReady)
        && (!shouldExpose3D68Landmarks(m_options.landmarkMode) || m_landmarker3dReady);
}

bool FaceRecognitionPipeline::reloadEmbeddings(const QString& embeddingsDir) {
    return m_database.load(embeddingsDir);
}

void FaceRecognitionPipeline::setOptions(const FaceRecognitionOptions& options) {
    m_options = options;
}

FaceRecognitionOptions FaceRecognitionPipeline::options() const {
    return m_options;
}

RecognitionResult FaceRecognitionPipeline::process(const cv::Mat& frame) {
    RecognitionResult result;
    if (frame.empty()) {
        return result;
    }

    const auto startedAt = std::chrono::steady_clock::now();

    float brightness = 0.0f;
    result.lowLight = isLowLight(frame, 70.0f, brightness);
    result.brightness = brightness;

    cv::Mat smallFrame;
    constexpr double scaleFactor = 0.5;
    cv::resize(frame, smallFrame, cv::Size(), scaleFactor, scaleFactor);

    auto faces = m_detector.detect(smallFrame, 0, "default");

    for (auto& face : faces) {
        face.bbox.x /= static_cast<float>(scaleFactor);
        face.bbox.y /= static_cast<float>(scaleFactor);
        face.bbox.width /= static_cast<float>(scaleFactor);
        face.bbox.height /= static_cast<float>(scaleFactor);

        for (auto& point : face.kps) {
            point.x /= static_cast<float>(scaleFactor);
            point.y /= static_cast<float>(scaleFactor);
        }

        if (shouldExpose106Landmarks(m_options.landmarkMode) && m_landmarkerReady) {
            const cv::Rect landmarkRect = face.bbox;
            face.landmarks106 = m_landmarker.detect(frame, landmarkRect);
        }

        if (shouldExpose3D68Landmarks(m_options.landmarkMode) && m_landmarker3dReady) {
            const cv::Rect landmarkRect = face.bbox;
            face.landmarks3d68 = m_landmarker3d68.detect(frame, landmarkRect);
        }

        const cv::Mat aligned = m_aligner.normCrop(frame, face.kps, 112);
        face.embedding = m_recognizer.extract(aligned);

        auto match = m_database.match(face.embedding, FaceMatchMinimumSimilarity);
        face.name = match.first;
        face.confidencePercent = match.second;
        face.similarity = (match.second / 50.0f) - 1.0f;
    }

    const auto finishedAt = std::chrono::steady_clock::now();
    const double totalMs = std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();

    result.faces = std::move(faces);
    result.fps = totalMs > 0.0 ? static_cast<float>(1000.0 / totalMs) : 0.0f;
    return result;
}

bool FaceRecognitionPipeline::isLowLight(const cv::Mat& frame, float threshold, float& brightness) {
    cv::Mat ycrcb;
    cv::cvtColor(frame, ycrcb, cv::COLOR_BGR2YCrCb);

    std::vector<cv::Mat> channels;
    cv::split(ycrcb, channels);

    brightness = static_cast<float>(cv::mean(channels[0])[0]);
    return brightness < threshold;
}
