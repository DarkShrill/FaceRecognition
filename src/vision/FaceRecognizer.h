#pragma once

#include <onnxruntime_cxx_api.h>
#include <QDebug>
#include <QString>
#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include "InferenceDevice.h"

/**
 * @brief Wrapper del modello ONNX che estrae embedding da volti allineati.
 *
 * Si occupa di configurare ONNX Runtime, caricare il modello, preprocessare
 * l'immagine e restituire il vettore embedding prodotto dall'inferenza.
 */
class FaceRecognizer {
public:
    /**
     * @brief Crea il recognizer richiedendo uno specifico device di inferenza.
     * @param device Device richiesto, CPU o CUDA.
     */
    FaceRecognizer(InferenceDevice device = InferenceDevice::CPU);

    /**
     * @brief Carica il modello ONNX di riconoscimento.
     * @param modelPath Path assoluto o relativo al file .onnx.
     * @return true se la sessione ONNX e stata creata correttamente.
     */
    bool load(const QString& modelPath);

    /**
     * @brief Estrae l'embedding da un volto gia allineato.
     * @param alignedFace Immagine BGR del volto, normalmente 112x112.
     * @return Vettore embedding. Vuoto se il modello non e caricato o l'input e vuoto.
     */
    std::vector<float> extract(const cv::Mat& alignedFace) const;

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

    Ort::Env m_env;
    std::unique_ptr<Ort::Session> m_session;
    Ort::SessionOptions m_sessionOptions;
    std::string m_inputName;
    std::string m_outputName;
    cv::Size m_inputSize{112, 112};
    float m_inputMean = 127.5f;
    float m_inputStd = 127.5f;
};
