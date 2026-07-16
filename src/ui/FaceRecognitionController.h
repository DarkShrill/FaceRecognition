#pragma once

#include "FaceRecognitionEngine.h"
#include "FaceEnrollmentExtractor.h"
#include "FrameImageProvider.h"
#include "FrameRenderer.h"

#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QStringList>
#include <QVector>
#include <opencv2/opencv.hpp>
#include <memory>

class QThread;

/**
 * @brief Controller esposto a QML per guidare QVideoStream, engine e overlay provider.
 *
 * E il punto di integrazione tra UI e backend: riceve frame QImage da
 * QVideoStream, li invia all'engine su thread dedicato, riceve RecognitionResult
 * e aggiorna l'overlay mostrato in QML tramite FrameImageProvider.
 */
class FaceRecognitionController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString frameSource READ frameSource NOTIFY frameSourceChanged)
    /** Stato testuale dell'applicazione, per esempio Ready, Loading models, Running. */
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    /** True quando l'engine e pronto a ricevere frame da QVideoStream. */
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    /** Numero di volti nell'ultimo risultato ricevuto dalla pipeline. */
    Q_PROPERTY(int faceCount READ faceCount NOTIFY metricsChanged)
    /** FPS della pipeline di inferenza calcolati sull'ultimo frame processato. */
    Q_PROPERTY(double fps READ fps NOTIFY metricsChanged)
    /** Luminosita media dell'ultimo frame processato. */
    Q_PROPERTY(double brightness READ brightness NOTIFY metricsChanged)
    /** True quando l'ultimo frame processato e sotto la soglia di luminosita. */
    Q_PROPERTY(bool lowLight READ lowLight NOTIFY metricsChanged)
    /** Modalita landmark: 0 none, 1 5 punti, 2 106 punti, 3 tutti. */
    Q_PROPERTY(int landmarkMode READ landmarkMode WRITE setLandmarkMode NOTIFY landmarkModeChanged)
    /** Device inferenza richiesto: 0 CPU, 1 CUDA con fallback CPU. */
    Q_PROPERTY(int inferenceDevice READ inferenceDevice WRITE setInferenceDevice NOTIFY inferenceDeviceChanged)
    /** URL sorgente per QVideoStream, per esempio video=Integrated Camera o rtsp://... */
    Q_PROPERTY(QString videoUrl READ videoUrl WRITE setVideoUrl NOTIFY videoUrlChanged)
    /** True durante la raccolta temporanea di immagini per una nuova persona. */
    Q_PROPERTY(bool enrolling READ enrolling NOTIFY enrollmentChanged)
    /** True mentre l'extractor C++ sta creando pkl/json da immagini temporanee. */
    Q_PROPERTY(bool extracting READ extracting NOTIFY enrollmentChanged)
    /** Numero di scatti gia salvati temporaneamente per la sessione corrente. */
    Q_PROPERTY(int enrollmentCapturedCount READ enrollmentCapturedCount NOTIFY enrollmentChanged)
    /** Numero di scatti richiesti dalla UI per la sessione corrente. */
    Q_PROPERTY(int enrollmentTargetCount READ enrollmentTargetCount NOTIFY enrollmentChanged)
    /** Stato testuale della sessione di adding new face. */
    Q_PROPERTY(QString enrollmentStatus READ enrollmentStatus NOTIFY enrollmentStatusChanged)
    /** Istruzione corrente per guidare la posa dell'utente durante l'enrollment. */
    Q_PROPERTY(QString enrollmentInstruction READ enrollmentInstruction NOTIFY enrollmentChanged)
    /** Impulso breve usato dalla UI per mostrare lo scatto. */
    Q_PROPERTY(bool captureFlash READ captureFlash NOTIFY captureFlashChanged)
    /** Nomi dei volti salvati negli embedding persistenti. */
    Q_PROPERTY(QStringList knownFaces READ knownFaces NOTIFY knownFacesChanged)
    /** Punto guida normalizzato X per la posa enrollment corrente, da -1 a 1. */
    Q_PROPERTY(double enrollmentGuideX READ enrollmentGuideX NOTIFY enrollmentChanged)
    /** Punto guida normalizzato Y per la posa enrollment corrente, da -1 a 1. */
    Q_PROPERTY(double enrollmentGuideY READ enrollmentGuideY NOTIFY enrollmentChanged)

public:
    /**
     * @brief Crea il controller e configura il worker thread.
     * @param provider Provider usato da QML per leggere i frame renderizzati.
     * @param parent Parent QObject opzionale.
     *
     * Il provider non viene posseduto dal controller: viene creato in main.cpp
     * e registrato anche nel QQmlApplicationEngine.
     */
    explicit FaceRecognitionController(FrameImageProvider* fr, QObject* parent = nullptr);

    /**
     * @brief Ferma analisi e worker thread prima della distruzione.
     */
    ~FaceRecognitionController() override;

    /**
     * @brief Stato testuale corrente esposto alla UI.
     */
    QString status() const;

    /**
     * @brief Indica se l'analisi dei frame in ingresso e attiva.
     */
    bool running() const;

    /**
     * @brief Numero di volti rilevati nell'ultimo risultato.
     */
    int faceCount() const;

    /**
     * @brief FPS di inferenza dell'ultimo risultato.
     */
    double fps() const;

    /**
     * @brief Luminosita media dell'ultimo risultato.
     */
    double brightness() const;

    /**
     * @brief Indica se l'ultimo risultato segnala bassa luminosita.
     */
    bool lowLight() const;

    /**
     * @brief Restituisce la modalita landmark corrente.
     */
    int landmarkMode() const;

    /**
     * @brief Cambia live quali landmark calcolare/disegnare.
     * @param mode 0 none, 1 5 punti, 2 106 punti, 3 tutti.
     */
    void setLandmarkMode(int mode);

    /**
     * @brief Restituisce il device di inferenza richiesto.
     */
    int inferenceDevice() const;

    /**
     * @brief Cambia device richiesto; se l'engine e attivo viene ricreato.
     * @param device 0 CPU, 1 CUDA.
     */
    void setInferenceDevice(int device);

    /**
     * @brief URL sorgente usato dalla demo QML con QVideoStream.
     */
    QString videoUrl() const;

    /**
     * @brief Aggiorna l'URL sorgente video usato dalla demo.
     */
    void setVideoUrl(const QString& videoUrl);
    bool autoSaveDetectedFaces() const;
    Q_INVOKABLE void setAutoSaveDetectedFaces(bool enabled);

    bool enrolling() const;
    bool extracting() const;
    int enrollmentCapturedCount() const;
    int enrollmentTargetCount() const;
    QString enrollmentStatus() const;
    QString enrollmentInstruction() const;
    bool captureFlash() const;
    QStringList knownFaces() const;
    double enrollmentGuideX() const;
    double enrollmentGuideY() const;

    /**
     * @brief Inizializza modelli e prepara l'engine.
     *
     * Viene chiamato da QML. La cattura video reale e demandata a QVideoStream.
     */
    Q_INVOKABLE void start();

    /**
     * @brief Ferma l'analisi e aggiorna lo stato applicativo.
     */
    Q_INVOKABLE void stop();

    /**
     * @brief Riceve un frame RGB da QVideoStream e lo invia alla libreria.
     * @param image Frame RGB generato dal decoder FFmpeg.
     */
    Q_INVOKABLE void submitVideoFrame(const QImage& image);

    /**
     * @brief Avvia una sessione di acquisizione per aggiungere una nuova persona.
     * @param personName Nome usato per pkl, metadata ed embeddings.json.
     */
    Q_INVOKABLE void startFaceEnrollment(const QString& personName);

    /**
     * @brief Crea un embedding medio partendo da una cartella di immagini gia esistenti.
     * @param personName Nome da associare alla persona.
     * @param folderPath Cartella contenente immagini con un solo volto.
     */
    Q_INVOKABLE void learnFaceFromFolder(const QString& personName, const QString& folderPath);

    /**
     * @brief Annulla la sessione e rimuove le immagini temporanee gia salvate.
     */
    Q_INVOKABLE void cancelFaceEnrollment();

    /**
     * @brief Rilegge embeddings.json e, se il motore e attivo, ricarica il database in memoria.
     */
    Q_INVOKABLE void refreshKnownFaces();

    /**
     * @brief Elimina un volto dagli embedding persistenti e ricarica il database.
     */
    Q_INVOKABLE bool deleteKnownFace(const QString& personName);


    QString frameSource() const;

signals:
    void frameSourceChanged();
    /** Emesso quando cambia status. */
    void statusChanged();
    /** Emesso quando cambia running. */
    void runningChanged();
    /** Emesso quando cambiano faceCount, fps, brightness o lowLight. */
    void metricsChanged();
    /** Emesso quando cambia la modalita landmark. */
    void landmarkModeChanged();
    /** Emesso quando cambia il device di inferenza richiesto. */
    void inferenceDeviceChanged();
    /** Emesso quando cambia l'URL sorgente video. */
    void videoUrlChanged();
    /** Emesso quando cambiano contatori o stato booleano della sessione enrollment. */
    void enrollmentChanged();
    /** Emesso quando cambia il messaggio di stato enrollment. */
    void enrollmentStatusChanged();
    /** Emesso quando la UI deve mostrare/nascondere il flash di scatto. */
    void captureFlashChanged();
    /** Emesso quando cambia la lista dei volti salvati. */
    void knownFacesChanged();
    /** Emesso con il risultato completo per renderer grafici esterni. */
    void analysisResultReady(RecognitionResult result);
    /** Emesso quando il risultato include i landmark richiesti dalla configurazione. */
    void landmarksReady(RecognitionResult result);

private slots:
    /**
     * @brief Riceve il risultato del worker e aggiorna metriche e overlay.
     * @param result Risultato calcolato dalla pipeline.
     */
    void handleResult(const RecognitionResult& result);

    /**
     * @brief Aggiorna lo stato locale di occupazione del worker.
     * @param busy true se il worker sta processando un frame.
     */
    void handleBusyChanged(bool busy);

private:
    /**
     * @brief Imposta status emettendo statusChanged solo se il valore cambia.
     */
    void setStatus(const QString& status);

    /**
     * @brief Imposta running emettendo runningChanged solo se il valore cambia.
     */
    void setRunning(bool running);

    static cv::Mat imageToBgrMat(const QImage& image);
    static QString sanitizePersonName(const QString& personName);
    static InferenceDevice sanitizeInferenceDevice(int device);
    void recreateEngine();
    enum class EnrollmentPose {
        Straight,
        YawRight15,
        YawLeft15,
        YawRight30,
        YawLeft30,
        YawRight60,
        YawLeft60,
        SlightUp,
        SlightDown
    };
    struct EnrollmentPoseStep {
        EnrollmentPose pose = EnrollmentPose::Straight;
        QString instruction;
    };

    QVector<EnrollmentPoseStep> buildEnrollmentPlan(int targetCount) const;
    QPointF enrollmentGuidePoint() const;
    bool currentPoseMatches(EnrollmentPose pose, QString* hint) const;
    void maybeCaptureEnrollmentFrame(const QImage& image);
    void runEnrollmentExtractor();
    void runEnrollmentExtractor(const QString& inputDir, const QString& personName, bool cleanupInputDir);
    void cleanupEnrollmentTempDir();
    void setEnrollmentStatus(const QString& status);
    void setCaptureFlash(bool flash);
    void setKnownFaces(const QStringList& faces);
    void maybeSaveDetectedFaces(const RecognitionResult& result, const QImage& frame);
    bool saveDetectedFaceImage(const QString& identity, const DetectedFace& face, const QImage& frame, int faceIndex);
    QString autoSaveIdentityForFace(const DetectedFace& face);

    RecognitionResult m_lastResult;
    bool m_workerBusy = false;
    bool m_running = false;
    int m_frameCount = 0;
    int m_skipFrames = 2;
    QString m_status = "Ready";
    QString m_videoUrl = "video=Full HD webcam";
    bool m_autoSaveDetectedFaces = false;
    QImage m_pendingAutoSaveImage;
    QHash<QString, QDateTime> m_lastAutoSaveByIdentity;

    bool m_enrolling = false;
    bool m_extracting = false;
    bool m_enrollmentCancelRequested = false;
    bool m_captureFlash = false;
    int m_enrollmentCapturedCount = 0;
    int m_enrollmentTargetCount = 0;
    QVector<EnrollmentPoseStep> m_enrollmentPlan;
    QString m_enrollmentPersonName;
    QString m_enrollmentSafeName;
    QString m_enrollmentTempDir;
    QString m_enrollmentStatus = "Ready";
    QElapsedTimer m_enrollmentTimer;
    QImage m_pendingEnrollmentImage;
    QThread* m_extractorThread = nullptr;
    QStringList m_knownFaces;
    InferenceDevice m_inferenceDevice = InferenceDevice::CPU;
    LandmarkMode m_landmarkMode = LandmarkMode::All;

    FrameImageProvider* m_provider = nullptr;
    FrameRenderer m_renderer;
    QSize m_lastFrameSize;
    int m_frameRevision = 0;

    std::unique_ptr<FaceRecognitionEngine> m_engine;
};
