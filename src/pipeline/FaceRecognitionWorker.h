#pragma once

#include "FaceRecognitionPipeline.h"
#include "InferenceDevice.h"

#include <QObject>
#include <atomic>

/**
 * @brief Adattatore QObject che esegue la pipeline su un thread dedicato.
 *
 * Il controller invia frame tramite queued connection. Il worker evita
 * elaborazioni concorrenti con m_busy e inoltra il risultato tramite signal.
 * La logica di riconoscimento resta in FaceRecognitionPipeline.
 */
class FaceRecognitionWorker : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Crea il worker e la pipeline interna.
     * @param device Device di inferenza richiesto.
     * @param parent Parent QObject opzionale.
     */
    explicit FaceRecognitionWorker(InferenceDevice device = InferenceDevice::CPU, QObject* parent = nullptr);

    /**
     * @brief Inizializza la pipeline interna.
     * @param detectorModel Path del modello detector.
     * @param landmarkModel Path del modello landmark a 106 punti.
     * @param landmark3dModel Path del modello landmark 3D a 68 punti.
     * @param recognizerModel Path del modello recognizer.
     * @param embeddingsDir Cartella degli embedding noti.
     * @return true se i modelli principali sono stati caricati correttamente.
     *
     * Deve essere chiamata nel thread del worker. Il controller usa una
     * BlockingQueuedConnection per garantire questa proprieta.
     */
    bool initialize(const QString& detectorModel,
                    const QString& landmarkModel,
                    const QString& landmark3dModel,
                    const QString& recognizerModel,
                    const QString& embeddingsDir);

    /**
     * @brief Ricarica il database embedding della pipeline.
     * @param embeddingsDir Cartella contenente gli embedding aggiornati.
     * @return true se almeno un embedding valido e stato caricato.
     */
    bool reloadEmbeddings(const QString& embeddingsDir);

public slots:
    /**
     * @brief Aggiorna le opzioni della pipeline nel thread del worker.
     * @param options Configurazione da usare dai prossimi frame.
     */
    void setOptions(FaceRecognitionOptions options);

    /**
     * @brief Processa un frame in modo asincrono rispetto al thread UI.
     * @param frame Copia del frame OpenCV da analizzare.
     *
     * Se il worker e gia occupato, la chiamata viene ignorata. Al termine
     * emette resultReady e aggiorna busyChanged.
     */
    void processFrame(cv::Mat frame);

signals:
    /**
     * @brief Emesso quando la pipeline produce un nuovo risultato.
     * @param result Risultato completo del frame processato.
     */
    void resultReady(const RecognitionResult& result);

    /**
     * @brief Emesso quando cambia lo stato di occupazione del worker.
     * @param busy true durante l'inferenza, false quando torna disponibile.
     */
    void busyChanged(bool busy);

private:
    FaceRecognitionPipeline m_pipeline;
    std::atomic_bool m_busy{false};
};
