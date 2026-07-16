#pragma once

#include "FaceTypes.h"
#include <onnxruntime_cxx_api.h>
#include <QDebug>
#include <QString>
#include <opencv2/opencv.hpp>
#include <array>
#include <memory>
#include <unordered_map>
#include "InferenceDevice.h"

/**
 * @brief Wrapper del modello SCRFD/ONNX usato per rilevare volti e landmark.
 *
 * Gestisce preprocess, inferenza ONNX, decodifica delle distanze modello,
 * filtro per score, NMS e conversione in DetectedFace.
 */
class FaceDetector {
public:
    /**
     * @brief Crea il detector richiedendo uno specifico device di inferenza.
     * @param device Device richiesto, CPU o CUDA.
     */
    FaceDetector(InferenceDevice device = InferenceDevice::CPU);

    /**
     * @brief Carica il modello detector e legge metadati input/output.
     * @param modelPath Path del file .onnx.
     * @param inputSize Dimensione input usata per il detector.
     * @return true se la sessione ONNX e pronta.
     */
    bool load(const QString& modelPath, const cv::Size& inputSize = cv::Size(640, 480));

    /**
     * @brief Rileva volti in un'immagine BGR.
     * @param image Immagine da analizzare.
     * @param maxNum Numero massimo di volti da restituire; 0 significa nessun limite.
     * @param metric Strategia di ranking quando maxNum e maggiore di 0.
     * @return Lista di DetectedFace con bounding box e landmark in coordinate immagine.
     */
    std::vector<DetectedFace> detect(const cv::Mat& image, int maxNum = 0, const QString& metric = "default") const;

    /**
     * @brief Device richiesto in costruzione.
     */
    InferenceDevice requestedDevice() const { return m_requestedDevice; }

    /**
     * @brief Device effettivamente usato dopo configureSessionOptions().
     */
    InferenceDevice actualDevice() const { return m_actualDevice; }

private:
    /**
     * @brief Configura provider e opzioni ONNX Runtime.
     * @return true se le opzioni sono state configurate; CUDA puo fare fallback a CPU.
     */
    bool configureSessionOptions();

private:
    InferenceDevice m_requestedDevice = InferenceDevice::CPU;
    InferenceDevice m_actualDevice = InferenceDevice::CPU;

    /**
     * @brief Output intermedio grezzo della forward SCRFD prima di ordinamento/NMS.
     */
    struct ForwardResult {
        std::vector<float> scores;
        std::vector<cv::Rect2f> bboxes;
        std::vector<std::array<cv::Point2f, 5>> kpss;
    };

    /**
     * @brief Chiave della cache degli anchor center per dimensione feature map e stride.
     */
    struct CenterCacheKey {
        int h;
        int w;
        int stride;

        /**
         * @brief Confronta due chiavi cache.
         */
        bool operator==(const CenterCacheKey& other) const {
            return h == other.h && w == other.w && stride == other.stride;
        }
    };

    /**
     * @brief Hash per usare CenterCacheKey in unordered_map.
     */
    struct CenterCacheKeyHash {
        /**
         * @brief Calcola un hash compatto da height, width e stride.
         */
        std::size_t operator()(const CenterCacheKey& k) const {
            return (static_cast<std::size_t>(k.h) << 20) ^ (static_cast<std::size_t>(k.w) << 8) ^ static_cast<std::size_t>(k.stride);
        }
    };

    /**
     * @brief Converte distanze landmark modello in punti immagine.
     * @param points Anchor center usati come origine.
     * @param distance Buffer distanze modello.
     * @param count Numero di anchor da decodificare.
     * @return Lista piatta di punti landmark.
     */
    static std::vector<cv::Point2f> distance2kps(const std::vector<cv::Point2f>& points, const float* distance, int count);

    /**
     * @brief Converte distanze bounding box modello in rettangoli.
     * @param points Anchor center usati come origine.
     * @param distance Buffer distanze modello.
     * @param count Numero di anchor da decodificare.
     * @return Bounding box decodificate.
     */
    static std::vector<cv::Rect2f> distance2bbox(const std::vector<cv::Point2f>& points, const float* distance, int count);

    /**
     * @brief Applica non-maximum suppression sulle bounding box.
     * @param boxes Bounding box candidate.
     * @param scores Score associati alle box.
     * @param threshold Soglia IoU oltre cui sopprimere box sovrapposte.
     * @return Indici delle box conservate.
     */
    static std::vector<int> nms(const std::vector<cv::Rect2f>& boxes, const std::vector<float>& scores, float threshold);

    /**
     * @brief Esegue preprocess e inferenza ONNX su immagine gia adattata all'input.
     * @param detImg Immagine padded/resized alla dimensione input detector.
     * @return Score, bounding box e landmark grezzi sopra soglia detector.
     */
    ForwardResult forward(const cv::Mat& detImg) const;

    /**
     * @brief Restituisce anchor center per una feature map, usando cache interna.
     * @param height Altezza feature map.
     * @param width Larghezza feature map.
     * @param stride Stride relativo al livello FPN.
     * @return Vettore di anchor center.
     */
    std::vector<cv::Point2f> getAnchorCenters(int height, int width, int stride) const;

    Ort::Env m_env;
    std::unique_ptr<Ort::Session> m_session;
    Ort::SessionOptions m_sessionOptions;
    cv::Size m_inputSize;
    QString m_inputName;
    std::vector<std::string> m_inputNameStorage;
    std::vector<std::string> m_outputNamesStorage;
    std::vector<const char*> m_outputNames;
    int m_fmc = 0;
    int m_numAnchors = 1;
    bool m_useKps = false;
    bool m_batched = false;
    float m_inputMean = 127.5f;
    float m_inputStd = 128.0f;
    float m_nmsThresh = 0.4f;
    float m_detThresh = 0.5f;
    std::vector<int> m_featStrideFpn;
    mutable std::unordered_map<CenterCacheKey, std::vector<cv::Point2f>, CenterCacheKeyHash> m_centerCache;
};
