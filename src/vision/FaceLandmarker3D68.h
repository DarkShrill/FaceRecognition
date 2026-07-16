#pragma once

#include "InferenceDevice.h"

#include <onnxruntime_cxx_api.h>
#include <QString>
#include <opencv2/opencv.hpp>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Wrapper del modello ONNX 1k3d68.
 *
 * Il modello lavora su un crop volto e restituisce landmark tridimensionali.
 * La classe converte x/y nelle coordinate del frame e conserva z come valore
 * relativo al crop normalizzato dal modello.
 */
class FaceLandmarker3D68 {
public:
    explicit FaceLandmarker3D68(InferenceDevice device = InferenceDevice::CPU);

    bool load(const QString& modelPath);
    bool isLoaded() const { return static_cast<bool>(m_session); }

    std::vector<cv::Point3f> detect(const cv::Mat& frame, const cv::Rect& faceRect) const;

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
