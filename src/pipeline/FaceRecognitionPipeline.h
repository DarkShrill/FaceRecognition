#pragma once

#include "EmbeddingDatabase.h"
#include "FaceAligner.h"
#include "FaceDetector.h"
#include "FaceLandmarker.h"
#include "FaceLandmarker3D68.h"
#include "FaceRecognizer.h"
#include "FaceTypes.h"
#include "InferenceDevice.h"

#include <QString>
#include <opencv2/opencv.hpp>

/**
 * @brief Pipeline sincrona di riconoscimento facciale su un singolo frame.
 *
 * Questa classe contiene la logica applicativa pura: detection, allineamento,
 * estrazione embedding, matching con il database e calcolo delle metriche.
 * Non conosce QML, timer, image provider o thread Qt.
 */
class FaceRecognitionPipeline {
public:
    /**
     * @brief Crea la pipeline richiedendo uno specifico backend di inferenza.
     * @param device Device desiderato per detector e recognizer.
     */
    explicit FaceRecognitionPipeline(InferenceDevice device = InferenceDevice::CPU);

    /**
     * @brief Carica modelli ONNX e database embedding.
     * @param detectorModel Path del modello detector, per esempio det_500m.onnx.
     * @param landmarkModel Path del modello landmark, per esempio face_landmark_2d_106.onnx.
     * @param landmark3dModel Path del modello landmark 3D, per esempio 1k3d68.onnx.
     * @param recognizerModel Path del modello recognizer, per esempio w600k_mbf.onnx.
     * @param embeddingsDir Cartella contenente embeddings.json o file JSON singoli.
     * @return true se detector, landmarker e recognizer sono stati caricati; il database puo
     *         essere vuoto ma viene comunque tentato il caricamento.
     */
    bool initialize(const QString& detectorModel,
                    const QString& landmarkModel,
                    const QString& landmark3dModel,
                    const QString& recognizerModel,
                    const QString& embeddingsDir);

    /**
     * @brief Ricarica il database embedding senza reinizializzare i modelli ONNX.
     * @param embeddingsDir Cartella contenente gli embedding aggiornati.
     * @return true se almeno un embedding valido e stato caricato.
     */
    bool reloadEmbeddings(const QString& embeddingsDir);

    /**
     * @brief Aggiorna le opzioni usate dai prossimi frame.
     * @param options Nuova configurazione runtime.
     *
     * Puo essere chiamata anche dopo initialize(): il frame gia in corso usa la
     * configurazione con cui e partito, i successivi usano quella nuova.
     */
    void setOptions(const FaceRecognitionOptions& options);

    /**
     * @brief Restituisce la configurazione corrente della pipeline.
     */
    FaceRecognitionOptions options() const;

    /**
     * @brief Esegue la pipeline completa su un frame BGR OpenCV.
     * @param frame Frame camera in formato OpenCV. Se vuoto produce un risultato vuoto.
     * @return RecognitionResult con volti, metriche, luminosita e FPS inferenza.
     */
    RecognitionResult process(const cv::Mat& frame);

private:
    /**
     * @brief Calcola se un frame e sotto la soglia di luminosita.
     * @param frame Frame BGR di input.
     * @param threshold Soglia sul valore medio del canale Y.
     * @param brightness Output: luminosita media calcolata.
     * @return true se brightness e minore di threshold.
     */
    static bool isLowLight(const cv::Mat& frame, float threshold, float& brightness);

    FaceDetector m_detector;
    FaceLandmarker m_landmarker;
    FaceLandmarker3D68 m_landmarker3d68;
    FaceAligner m_aligner;
    FaceRecognizer m_recognizer;
    EmbeddingDatabase m_database;
    FaceRecognitionOptions m_options;
    bool m_landmarkerReady = false;
    bool m_landmarker3dReady = false;
};
