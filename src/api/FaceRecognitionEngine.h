#pragma once

#include "FaceRecognitionGlobal.h"
#include "FaceTypes.h"
#include "InferenceDevice.h"

#include <QObject>
#include <QThread>
#include <opencv2/opencv.hpp>

class FaceRecognitionWorker;

/**
 * @brief Facade QObject pensato per usare il riconoscimento come libreria.
 *
 * La classe non apre webcam e non renderizza immagini: riceve frame dall'host
 * tramite submitFrame()/processFrame(), li inoltra al worker thread e pubblica
 * i risultati con signal Qt. In questo modo l'applicazione che integra la
 * libreria decide da dove arrivano i fotogrammi e come disegnarli.
 */
class FACERECOGNITION_EXPORT FaceRecognitionEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(int landmarkMode READ landmarkMode WRITE setLandmarkMode NOTIFY optionsChanged)
    Q_PROPERTY(bool initialized READ isInitialized NOTIFY initializedChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)

public:
    /**
     * @brief Crea il facade e avvia il worker thread interno.
     * @param device Device di inferenza richiesto.
     * @param parent Parent QObject opzionale.
     */
    explicit FaceRecognitionEngine(InferenceDevice device = InferenceDevice::CPU, QObject* parent = nullptr);
    ~FaceRecognitionEngine() override;

    /**
     * @brief Inizializza modelli e database embedding nel thread del worker.
     * @return true se i modelli principali sono pronti.
     */
    bool initialize(const QString& detectorModel,
                    const QString& landmarkModel,
                    const QString& landmark3dModel,
                    const QString& recognizerModel,
                    const QString& embeddingsDir);

    /**
     * @brief Ricarica gli embedding noti senza fermare il worker o riaprire i modelli.
     */
    bool reloadEmbeddings(const QString& embeddingsDir);

    /**
     * @brief Restituisce il LandmarkMode corrente come int per QML/API semplici.
     */
    int landmarkMode() const;

    /**
     * @brief Indica se initialize() e riuscito.
     */
    bool isInitialized() const;

    /**
     * @brief Indica se il worker sta analizzando un frame.
     */
    bool isBusy() const;

public slots:
    /**
     * @brief Aggiorna live quali landmark calcolare/esporre.
     * @param mode Valore di LandmarkMode convertito a int.
     */
    void setLandmarkMode(int mode);

    /**
     * @brief Slot principale per consegnare frame alla libreria.
     * @param frame Frame OpenCV BGR. Viene copiato prima di attraversare il thread.
     */
    void submitFrame(cv::Mat frame);

signals:
    /** Emesso quando cambia la configurazione runtime. */
    void optionsChanged();
    /** Emesso dopo initialize() quando cambia lo stato inizializzato/non inizializzato. */
    void initializedChanged();
    /** Emesso quando il worker entra/esce da una inferenza. */
    void busyChanged(bool busy);
    /** Emesso quando un frame valido viene accettato per l'analisi. */
    void frameSubmitted(cv::Mat frame);
    /** Emesso con il risultato completo pronto per UI, logica applicativa o renderer custom. */
    void resultReady(RecognitionResult result);
    /** Emesso quando il risultato contiene dati landmark coerenti con landmarkMode. */
    void landmarksReady(RecognitionResult result);

private:
    void applyOptions();
    static LandmarkMode sanitizeLandmarkMode(int mode);

    QThread m_workerThread;
    FaceRecognitionWorker* m_worker = nullptr;
    FaceRecognitionOptions m_options;
    bool m_initialized = false;
    bool m_busy = false;
};
