#pragma once

#include "FaceRecognitionGlobal.h"
#include "FaceAligner.h"
#include "FaceDetector.h"
#include "FaceRecognizer.h"
#include "InferenceDevice.h"

#include <QString>
#include <opencv2/opencv.hpp>
#include <vector>

struct FACERECOGNITION_EXPORT FaceEnrollmentResult {
    bool ok = false;
    QString message;
    int validImages = 0;
    int skippedImages = 0;
};

/**
 * @brief Extractor C++ per aggiungere una nuova persona da immagini temporanee.
 *
 * Usa solo immagini
 * con esattamente un volto, allinea il volto, estrae l'embedding e salva la
 * media in face_embeddings.
 */
class FACERECOGNITION_EXPORT FaceEnrollmentExtractor {
public:
    explicit FaceEnrollmentExtractor(InferenceDevice device = InferenceDevice::CPU);

    bool initialize(const QString& detectorModel, const QString& recognizerModel);

    FaceEnrollmentResult extract(const QString& inputDir,
                                 const QString& personName,
                                 const QString& saveDir,
                                 bool replaceExisting = true);

private:
    static QString safeFileStem(const QString& personName);
    static bool writePickleFloatList(const QString& filePath, const std::vector<float>& embedding, QString* error);
    static bool updateMetadata(const QString& saveDir, const QString& personName, QString* error);
    static bool updateEmbeddingsJson(const QString& saveDir,
                                     const QString& personName,
                                     const std::vector<float>& embedding,
                                     bool replaceExisting,
                                     QString* error);
    std::vector<float> extractOne(const cv::Mat& image, bool* exactlyOneFace) const;

    FaceDetector m_detector;
    FaceAligner m_aligner;
    FaceRecognizer m_recognizer;
    bool m_ready = false;
};
