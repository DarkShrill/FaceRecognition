#pragma once

#include "InferenceDevice.h"

#include <onnxruntime_cxx_api.h>
#include <QString>
#include <opencv2/opencv.hpp>
#include <array>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Wrapper del modello ONNX face_landmark_2d_106.
 *
 * Il modello lavora su un crop volto 192x192 e produce 106 coppie x/y
 * normalizzate. Questa classe converte quei punti nelle coordinate del frame.
 */
class FaceLandmarker {
public:
    /**
     * @brief Crea il landmarker richiedendo uno specifico device di inferenza.
     * @param device Device richiesto, CPU o CUDA.
     */
    explicit FaceLandmarker(InferenceDevice device = InferenceDevice::CPU);

    /**
     * @brief Carica il modello ONNX dei 106 landmark.
     * @param modelPath Path assoluto o relativo al file .onnx.
     * @return true se la sessione ONNX e pronta.
     */
    bool load(const QString& modelPath);

    /**
     * @brief Indica se il modello e stato caricato.
     */
    bool isLoaded() const { return static_cast<bool>(m_session); }

    /**
     * @brief Stima 106 landmark su un volto gia rilevato.
     * @param frame Frame BGR originale.
     * @param faceRect Bounding box del volto in coordinate frame.
     * @return Lista di 106 punti in coordinate frame. Vuota in caso di errore.
     */
    std::vector<cv::Point2f> detect(const cv::Mat& frame, const cv::Rect& faceRect) const;

    /**
     * @brief Estrae i 5 punti standard usati dall'allineamento ArcFace.
     * @param landmarks106 Landmark prodotti da detect().
     * @return Occhio sx, occhio dx, naso, bocca sx, bocca dx.
     */
    std::array<cv::Point2f, 5> toFivePoints(const std::vector<cv::Point2f>& landmarks106) const;

private:
    bool configureSessionOptions();

    InferenceDevice m_requestedDevice = InferenceDevice::CPU;
    InferenceDevice m_actualDevice = InferenceDevice::CPU;

    Ort::Env m_env;
    std::unique_ptr<Ort::Session> m_session;
    Ort::SessionOptions m_sessionOptions;
    std::string m_inputName;
    std::string m_outputName;
    cv::Size m_inputSize{192, 192};
};
