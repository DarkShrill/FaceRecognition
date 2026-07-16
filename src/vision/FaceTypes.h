#pragma once

#include <QString>
#include <QMetaType>
#include <opencv2/opencv.hpp>
#include <array>
#include <vector>

/**
 * @brief Modalita di esposizione/disegno dei landmark.
 *
 * La pipeline usa sempre i 5 keypoint del detector per l'allineamento interno.
 * Questa opzione controlla invece quali landmark completi calcolare e quali
 * punti il renderer deve mostrare.
 */
enum class LandmarkMode {
    None = 0,
    FivePoints = 1,
    Points106 = 2,
    All = 3,
    Points3D68 = 4
};

/**
 * @brief Opzioni runtime della pipeline riusabile.
 *
 * La struttura e intenzionalmente piccola: puo essere passata al worker da UI,
 * applicazioni host o futuri wrapper di libreria senza trascinarsi dipendenze
 * grafiche.
 */
struct FaceRecognitionOptions {
    LandmarkMode landmarkMode = LandmarkMode::All;
};

constexpr float FaceMatchMinimumConfidencePercent = 80.0f;
constexpr float FaceMatchMinimumSimilarity = (FaceMatchMinimumConfidencePercent / 50.0f) - 1.0f;

inline bool shouldExposeFivePointLandmarks(LandmarkMode mode) {
    return mode == LandmarkMode::FivePoints || mode == LandmarkMode::All;
}

inline bool shouldExpose106Landmarks(LandmarkMode mode) {
    return mode == LandmarkMode::Points106 || mode == LandmarkMode::All;
}

inline bool shouldExpose3D68Landmarks(LandmarkMode mode) {
    return mode == LandmarkMode::Points3D68 || mode == LandmarkMode::All;
}

/**
 * @brief Risultato relativo a un singolo volto rilevato in un frame.
 *
 * La struttura viaggia lungo tutta la pipeline: il detector compila bounding
 * box e landmark, il recognizer aggiunge l'embedding e il database completa
 * nome, similarita e confidence.
 */
struct DetectedFace {
    /** Bounding box del volto nelle coordinate del frame originale. */
    cv::Rect2f bbox;
    /** Cinque landmark facciali usati per l'allineamento del volto. */
    std::array<cv::Point2f, 5> kps{};
    /** Landmark facciali completi prodotti dal modello face_landmark_2d_106. */
    std::vector<cv::Point2f> landmarks106;
    /** Landmark 3D a 68 punti prodotti dal modello 1k3d68; x/y sono coordinate frame, z e relativo al crop. */
    std::vector<cv::Point3f> landmarks3d68;
    /** Vettore embedding estratto dal modello di riconoscimento. */
    std::vector<float> embedding;
    /** Nome del volto riconosciuto, oppure "Unknown". */
    QString name;
    /** Similarita normalizzata ricavata dalla confidence del match. */
    float similarity = 0.0f;
    /** Confidence percentuale mostrata nella UI. */
    float confidencePercent = 0.0f;
};

/**
 * @brief Risultato completo della pipeline su un frame.
 *
 * Viene emesso dal worker verso il controller e usato dalla UI per metriche,
 * overlay e stato visivo.
 */
struct RecognitionResult {
    /** Lista dei volti rilevati e, se possibile, riconosciuti. */
    std::vector<DetectedFace> faces;
    /** Frame per secondo della sola inferenza/pipeline. */
    float fps = 0.0f;
    /** True quando la luminosita media del frame e sotto soglia. */
    bool lowLight = false;
    /** Luminosita media del frame calcolata sul canale Y. */
    float brightness = 0.0f;
};

Q_DECLARE_METATYPE(cv::Mat)
Q_DECLARE_METATYPE(LandmarkMode)
Q_DECLARE_METATYPE(FaceRecognitionOptions)
Q_DECLARE_METATYPE(RecognitionResult)
