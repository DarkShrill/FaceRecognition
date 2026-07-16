#include "FaceRecognitionController.h"
#include "RuntimePaths.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QtMath>
#include <memory>

namespace {
constexpr qint64 AutoSaveMinimumIntervalMs = 3000;

bool isUnknownFaceName(const QString& name) {
    return name.trimmed().isEmpty() || name.compare(QStringLiteral("Unknown"), Qt::CaseInsensitive) == 0;
}

QString normalizedFolderPath(const QString& folderPath) {
    QString input = folderPath.trimmed();
    if ((input.startsWith('"') && input.endsWith('"')) ||
        (input.startsWith('\'') && input.endsWith('\''))) {
        input = input.mid(1, input.size() - 2).trimmed();
    }

    QString cleaned;
    cleaned.reserve(input.size());
    for (const QChar ch : input) {
        if (ch.category() != QChar::Other_Format) {
            cleaned.append(ch);
        }
    }
    input = cleaned.trimmed();

    if (input.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) {
        const QUrl fileUrl(input);
        if (fileUrl.isLocalFile()) {
            input = fileUrl.toLocalFile();
        }
    }

    const QFileInfo info(input);
    if (info.exists() && info.isDir()) {
        return QDir::fromNativeSeparators(info.absoluteFilePath());
    }
    return QDir::fromNativeSeparators(input);
}
}

FaceRecognitionController::FaceRecognitionController(FrameImageProvider *fr, QObject* parent)
    : QObject(parent),
      m_provider(fr) {
    recreateEngine();
    refreshKnownFaces();
}

FaceRecognitionController::~FaceRecognitionController() {
    if (m_extractorThread) {
        m_enrollmentCancelRequested = true;
        m_extractorThread->quit();
        m_extractorThread->wait();
    }
    cleanupEnrollmentTempDir();
    stop();
}

QString FaceRecognitionController::status() const {
    return m_status;
}

bool FaceRecognitionController::running() const {
    return m_running;
}

int FaceRecognitionController::faceCount() const {
    return static_cast<int>(m_lastResult.faces.size());
}

double FaceRecognitionController::fps() const {
    return m_lastResult.fps;
}

double FaceRecognitionController::brightness() const {
    return m_lastResult.brightness;
}

bool FaceRecognitionController::lowLight() const {
    return m_lastResult.lowLight;
}

int FaceRecognitionController::landmarkMode() const {
    return static_cast<int>(m_landmarkMode);
}

void FaceRecognitionController::setLandmarkMode(int mode) {
    if (m_engine) {
        m_engine->setLandmarkMode(mode);
        m_landmarkMode = static_cast<LandmarkMode>(m_engine->landmarkMode());
        return;
    }

    m_landmarkMode = static_cast<LandmarkMode>(mode);
    emit landmarkModeChanged();
}

int FaceRecognitionController::inferenceDevice() const {
    return m_inferenceDevice == InferenceDevice::CUDA ? 1 : 0;
}

void FaceRecognitionController::setInferenceDevice(int device) {
    const InferenceDevice requestedDevice = sanitizeInferenceDevice(device);
    if (m_inferenceDevice == requestedDevice) {
        return;
    }

    const bool restart = m_running;
    if (restart) {
        setRunning(false);
        setStatus("Switching inference device");
    }

    m_inferenceDevice = requestedDevice;
    recreateEngine();
    qInfo().nospace() << "FaceRecognitionController: Requested " << inferenceDeviceName(m_inferenceDevice);
    emit inferenceDeviceChanged();

    if (restart) {
        start();
    }
}

QString FaceRecognitionController::videoUrl() const {
    return m_videoUrl;
}

void FaceRecognitionController::setVideoUrl(const QString& videoUrl) {
    if (m_videoUrl == videoUrl) {
        return;
    }
    m_videoUrl = videoUrl;
    emit videoUrlChanged();
}

bool FaceRecognitionController::autoSaveDetectedFaces() const {
    return m_autoSaveDetectedFaces;
}

void FaceRecognitionController::setAutoSaveDetectedFaces(bool enabled) {
    if (m_autoSaveDetectedFaces == enabled) {
        return;
    }

    m_autoSaveDetectedFaces = enabled;
    if (!enabled) {
        m_pendingAutoSaveImage = QImage();
    }
}

bool FaceRecognitionController::enrolling() const {
    return m_enrolling;
}

bool FaceRecognitionController::extracting() const {
    return m_extracting;
}

int FaceRecognitionController::enrollmentCapturedCount() const {
    return m_enrollmentCapturedCount;
}

int FaceRecognitionController::enrollmentTargetCount() const {
    return m_enrollmentTargetCount;
}

QString FaceRecognitionController::enrollmentStatus() const {
    return m_enrollmentStatus;
}

QString FaceRecognitionController::enrollmentInstruction() const {
    if (!m_enrolling) {
        return m_extracting ? "Sto creando l'embedding, attendere..." : "Premi Capture e muovi la testa nel cerchio";
    }

    if (m_enrollmentCapturedCount < 0 || m_enrollmentCapturedCount >= m_enrollmentPlan.size()) {
        return "Mantieni il volto visibile";
    }

    return m_enrollmentPlan[m_enrollmentCapturedCount].instruction;
}

bool FaceRecognitionController::captureFlash() const {
    return m_captureFlash;
}

QStringList FaceRecognitionController::knownFaces() const {
    return m_knownFaces;
}

double FaceRecognitionController::enrollmentGuideX() const {
    return enrollmentGuidePoint().x();
}

double FaceRecognitionController::enrollmentGuideY() const {
    return enrollmentGuidePoint().y();
}

void FaceRecognitionController::start() {
    if (m_running) {
        return;
    }

    const QString detectorPath = RuntimePaths::resolve("models/det_500m.onnx");
    const QString landmarkPath = RuntimePaths::resolve("models/2d106det.onnx");
    const QString landmark3dPath = RuntimePaths::resolve("models/1k3d68.onnx");
    const QString recognizerPath = RuntimePaths::resolve("models/w600k_mbf.onnx");//w600k_r50
    const QString embeddingsDir = RuntimePaths::resolve("face_embeddings");

    setStatus("Loading models");
    qInfo().nospace() << "FaceRecognitionController: initializing models. Requested "
                      << inferenceDeviceName(m_inferenceDevice);
    const bool initOk = m_engine && m_engine->initialize(detectorPath, landmarkPath, landmark3dPath, recognizerPath, embeddingsDir);
    if (!initOk) {
        setStatus("Failed to load ONNX models");
        return;
    }

    m_frameCount = 0;
    m_lastResult = RecognitionResult{};
    setRunning(true);
    setStatus("Running");
    emit metricsChanged();
}

void FaceRecognitionController::stop() {
    setRunning(false);
    if (m_status == "Running") {
        setStatus("Stopped");
    }
}

void FaceRecognitionController::submitVideoFrame(const QImage& image) {
    if (!m_running || image.isNull()) {
        return;
    }

    const cv::Mat frame = imageToBgrMat(image);
    if (frame.empty()) {
        return;
    }

    m_lastFrameSize = image.size();

    ++m_frameCount;
    if (m_engine && m_engine->isInitialized() && !m_workerBusy && (m_frameCount % m_skipFrames == 0)) {
        m_pendingEnrollmentImage = m_enrolling ? image.copy() : QImage();
        m_pendingAutoSaveImage = m_autoSaveDetectedFaces ? image.copy() : QImage();
        m_engine->submitFrame(frame);
    }
}

void FaceRecognitionController::startFaceEnrollment(const QString& personName) {
    if (m_enrolling || m_extracting) {
        return;
    }

    constexpr int EnrollmentImageCount = 15;
    const QString trimmedName = personName.trimmed();
    const QString safeName = sanitizePersonName(trimmedName);
    if (trimmedName.isEmpty() || safeName.isEmpty()) {
        setEnrollmentStatus("Insert a valid name");
        return;
    }

    cleanupEnrollmentTempDir();

    const QString timestamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd_hhmmss_zzz");
    const QString tempRoot = RuntimePaths::resolve("tmp/add_faces");
    QDir rootDir(tempRoot);
    if (!rootDir.exists() && !rootDir.mkpath(".")) {
        setEnrollmentStatus("Cannot create temp folder");
        return;
    }

    m_enrollmentTempDir = rootDir.filePath(QString("%1_%2").arg(safeName, timestamp));
    QDir sessionDir(m_enrollmentTempDir);
    if (!sessionDir.exists() && !sessionDir.mkpath(".")) {
        m_enrollmentTempDir.clear();
        setEnrollmentStatus("Cannot create capture folder");
        return;
    }

    m_enrollmentPersonName = trimmedName;
    m_enrollmentSafeName = safeName;
    m_enrollmentCancelRequested = false;
    m_enrollmentCapturedCount = 0;
    m_enrollmentTargetCount = EnrollmentImageCount;
    m_enrollmentPlan = buildEnrollmentPlan(EnrollmentImageCount);
    m_enrolling = true;
    m_enrollmentTimer.invalidate();
    setEnrollmentStatus(QString("Capturing 0/%1 - %2").arg(m_enrollmentTargetCount).arg(enrollmentInstruction()));
    emit enrollmentChanged();
}

void FaceRecognitionController::cancelFaceEnrollment() {
    m_enrollmentCancelRequested = true;

    m_enrolling = false;
    if (m_extracting) {
        setEnrollmentStatus("Cancel requested");
        emit enrollmentChanged();
        return;
    }

    m_enrollmentCapturedCount = 0;
    m_enrollmentTargetCount = 0;
    m_enrollmentPlan.clear();
    cleanupEnrollmentTempDir();
    setCaptureFlash(false);
    setEnrollmentStatus("Canceled");
    emit enrollmentChanged();
}

void FaceRecognitionController::learnFaceFromFolder(const QString& personName, const QString& folderPath) {
    if (m_enrolling || m_extracting) {
        return;
    }

    const QString trimmedName = personName.trimmed();
    const QString safeName = sanitizePersonName(trimmedName);
    if (trimmedName.isEmpty() || safeName.isEmpty()) {
        setEnrollmentStatus("Insert a valid name");
        return;
    }

    const QString inputDir = normalizedFolderPath(folderPath);
    QDir dir(inputDir);
    if (inputDir.isEmpty() || !dir.exists()) {
        setEnrollmentStatus("Folder not found");
        return;
    }

    const QStringList imageFiles = dir.entryList({"*.jpg", "*.jpeg", "*.png", "*.bmp"}, QDir::Files);
    if (imageFiles.isEmpty()) {
        setEnrollmentStatus("Folder contains no images");
        return;
    }

    setEnrollmentStatus("Loading face folder");
    runEnrollmentExtractor(inputDir, trimmedName, false);
}

void FaceRecognitionController::refreshKnownFaces() {
    const QString embeddingsDir = RuntimePaths::resolve("face_embeddings");
    QDir dir(embeddingsDir);
    QSet<QString> names;

    QFile embeddingsFile(dir.filePath("embeddings.json"));
    if (embeddingsFile.exists() && embeddingsFile.open(QIODevice::ReadOnly)) {
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(embeddingsFile.readAll(), &error);
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonArray faces = doc.object().value("faces").toArray();
            for (const QJsonValue& value : faces) {
                if (!value.isObject()) {
                    continue;
                }

                const QJsonObject object = value.toObject();
                const QString name = object.value("name").toString().trimmed();
                if (!name.isEmpty() && !object.value("embedding").toArray().isEmpty()) {
                    names.insert(name);
                }
            }
        } else {
            qWarning() << "FaceRecognitionController: invalid embeddings.json" << error.errorString();
        }
    }

    if (m_engine && m_engine->isInitialized()) {
        m_engine->reloadEmbeddings(embeddingsDir);
    }

    QStringList sorted = names.values();
    sorted.sort(Qt::CaseInsensitive);
    setKnownFaces(sorted);
}

bool FaceRecognitionController::deleteKnownFace(const QString& personName) {
    const QString trimmedName = personName.trimmed();
    if (trimmedName.isEmpty()) {
        return false;
    }

    const QString embeddingsDir = RuntimePaths::resolve("face_embeddings");
    QDir dir(embeddingsDir);
    if (!dir.exists()) {
        return false;
    }

    bool changed = false;
    const QString embeddingsPath = dir.filePath("embeddings.json");
    QFile embeddingsInput(embeddingsPath);
    QJsonArray keptFaces;
    if (embeddingsInput.exists() && embeddingsInput.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(embeddingsInput.readAll());
        const QJsonArray faces = doc.object().value("faces").toArray();
        for (const QJsonValue& value : faces) {
            const QJsonObject object = value.toObject();
            if (object.value("name").toString() == trimmedName) {
                changed = true;
                continue;
            }
            keptFaces.append(object);
        }
        embeddingsInput.close();

        QFile embeddingsOutput(embeddingsPath);
        if (embeddingsOutput.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            embeddingsOutput.write(QJsonDocument(QJsonObject{{"faces", keptFaces}}).toJson(QJsonDocument::Indented));
        }
    }

    const QString metadataPath = dir.filePath("metadata.json");
    QFile metadataInput(metadataPath);
    QJsonArray keptNames;
    if (metadataInput.exists() && metadataInput.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(metadataInput.readAll());
        const QJsonArray names = doc.object().value("names").toArray();
        for (const QJsonValue& value : names) {
            const QString name = value.toString();
            if (name == trimmedName) {
                changed = true;
                continue;
            }
            keptNames.append(name);
        }
        metadataInput.close();

        QFile metadataOutput(metadataPath);
        if (metadataOutput.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            metadataOutput.write(QJsonDocument(QJsonObject{{"names", keptNames}}).toJson(QJsonDocument::Indented));
        }
    }

    const QString safeName = sanitizePersonName(trimmedName);
    const QStringList candidateFiles = {
        dir.filePath(trimmedName + ".pkl"),
        dir.filePath(safeName + ".pkl")
    };
    for (const QString& path : candidateFiles) {
        QFile file(path);
        if (file.exists() && file.remove()) {
            changed = true;
        }
    }

    refreshKnownFaces();
    if (m_engine && m_engine->isInitialized()) {
        m_engine->reloadEmbeddings(embeddingsDir);
    }
    return changed;
}

void FaceRecognitionController::handleResult(const RecognitionResult& result) {
    m_lastResult = result;

    if (m_provider && m_lastFrameSize.isValid()) {
        const auto landmarkMode = m_landmarkMode;
        const QImage overlay = m_renderer.renderOverlay(m_lastFrameSize, m_lastResult, landmarkMode);
        m_provider->setImage(overlay);
        ++m_frameRevision;
        emit frameSourceChanged();
    }

    emit analysisResultReady(result);
    emit metricsChanged();

    if (m_autoSaveDetectedFaces && !m_pendingAutoSaveImage.isNull()) {
        maybeSaveDetectedFaces(m_lastResult, m_pendingAutoSaveImage);
        m_pendingAutoSaveImage = QImage();
    }

    if (!m_pendingEnrollmentImage.isNull()) {
        maybeCaptureEnrollmentFrame(m_pendingEnrollmentImage);
        m_pendingEnrollmentImage = QImage();
    }
}

void FaceRecognitionController::handleBusyChanged(bool busy) {
    m_workerBusy = busy;
}

void FaceRecognitionController::setStatus(const QString& status) {
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

QString FaceRecognitionController::sanitizePersonName(const QString& personName) {
    QString safe = personName.trimmed();
    safe.replace(QRegularExpression("\\s+"), "_");
    safe.replace(QRegularExpression("[^A-Za-z0-9_\\-]"), "");
    return safe.left(80);
}

QVector<FaceRecognitionController::EnrollmentPoseStep> FaceRecognitionController::buildEnrollmentPlan(int targetCount) const {
    QVector<EnrollmentPoseStep> plan;
    plan.reserve(targetCount);

    const QVector<EnrollmentPoseStep> scan = {
        {EnrollmentPose::Straight, "Centra il volto nel riquadro"},
        {EnrollmentPose::YawRight15, "Ruota appena verso destra, circa 15 gradi"},
        {EnrollmentPose::YawLeft15, "Ruota appena verso sinistra, circa 15 gradi"},
        {EnrollmentPose::YawRight30, "Continua verso destra, circa 30 gradi"},
        {EnrollmentPose::YawLeft30, "Continua verso sinistra, circa 30 gradi"},
        {EnrollmentPose::SlightUp, "Alza leggermente il volto, senza perdere gli occhi"},
        {EnrollmentPose::SlightDown, "Abbassa leggermente il volto, senza guardare troppo giu"},
        {EnrollmentPose::YawRight60, "Massimo verso destra: resta sotto circa 60 gradi"},
        {EnrollmentPose::YawLeft60, "Massimo verso sinistra: resta sotto circa 60 gradi"},
        {EnrollmentPose::Straight, "Torna al centro"}
    };

    for (int i = 0; plan.size() < targetCount; ++i) {
        plan.push_back(scan[i % scan.size()]);
    }

    return plan;
}

QPointF FaceRecognitionController::enrollmentGuidePoint() const {
    if (!m_enrolling ||
        m_enrollmentCapturedCount < 0 ||
        m_enrollmentCapturedCount >= m_enrollmentPlan.size()) {
        return QPointF(0.0, 0.0);
    }

    switch (m_enrollmentPlan[m_enrollmentCapturedCount].pose) {
    case EnrollmentPose::Straight:
        return QPointF(0.0, 0.0);
    case EnrollmentPose::YawRight15:
        return QPointF(0.22, 0.0);
    case EnrollmentPose::YawLeft15:
        return QPointF(-0.22, 0.0);
    case EnrollmentPose::YawRight30:
        return QPointF(0.42, 0.0);
    case EnrollmentPose::YawLeft30:
        return QPointF(-0.42, 0.0);
    case EnrollmentPose::YawRight60:
        return QPointF(0.68, 0.0);
    case EnrollmentPose::YawLeft60:
        return QPointF(-0.68, 0.0);
    case EnrollmentPose::SlightUp:
        return QPointF(0.0, -0.28);
    case EnrollmentPose::SlightDown:
        return QPointF(0.0, 0.28);
    }

    return QPointF(0.0, 0.0);
}

bool FaceRecognitionController::currentPoseMatches(EnrollmentPose pose, QString* hint) const {
    if (m_lastResult.faces.size() != 1) {
        if (hint) {
            *hint = "Posizionati con un solo volto visibile";
        }
        return false;
    }

    const DetectedFace& face = m_lastResult.faces.front();
    if (face.bbox.width <= 0.0f || face.bbox.height <= 0.0f) {
        if (hint) {
            *hint = "Avvicina il volto alla camera";
        }
        return false;
    }

    const cv::Point2f leftEye = face.kps[0];
    const cv::Point2f rightEye = face.kps[1];
    const cv::Point2f nose = face.kps[2];
    const cv::Point2f leftMouth = face.kps[3];
    const cv::Point2f rightMouth = face.kps[4];
    const auto pointInFace = [&](const cv::Point2f& point) {
        const float marginX = face.bbox.width * 0.04f;
        const float marginY = face.bbox.height * 0.04f;
        return point.x >= face.bbox.x - marginX &&
               point.x <= face.bbox.x + face.bbox.width + marginX &&
               point.y >= face.bbox.y - marginY &&
               point.y <= face.bbox.y + face.bbox.height + marginY;
    };
    const float eyeDistance = qAbs(rightEye.x - leftEye.x);
    if (!pointInFace(leftEye) || !pointInFace(rightEye) || eyeDistance < face.bbox.width * 0.18f) {
        if (hint) {
            *hint = "Tieni entrambi gli occhi ben visibili";
        }
        return false;
    }

    const cv::Point2f eyeCenter((leftEye.x + rightEye.x) * 0.5f, (leftEye.y + rightEye.y) * 0.5f);
    const cv::Point2f mouthCenter((leftMouth.x + rightMouth.x) * 0.5f, (leftMouth.y + rightMouth.y) * 0.5f);
    const cv::Point2f faceCenter((eyeCenter.x + mouthCenter.x) * 0.5f, (eyeCenter.y + mouthCenter.y) * 0.5f);

    const float nx = (nose.x - faceCenter.x) / face.bbox.width;
    const float ny = (nose.y - faceCenter.y) / face.bbox.height;
    if (qAbs(nx) > 0.20f || qAbs(ny) > 0.10f) {
        if (hint) {
            *hint = "Evita pose troppo di lato o troppo in basso";
        }
        return false;
    }

    const auto inRange = [](float value, float minValue, float maxValue) {
        return value >= minValue && value <= maxValue;
    };
    const auto yawMatches = [&](float minAbsX, float maxAbsX, int side) {
        const float signedX = nx * static_cast<float>(side);
        return inRange(signedX, minAbsX, maxAbsX) && qAbs(ny) <= 0.070f;
    };
    bool matches = false;

    switch (pose) {
    case EnrollmentPose::Straight:
        matches = qAbs(nx) <= 0.035f && qAbs(ny) <= 0.040f;
        break;
    case EnrollmentPose::YawRight15:
        matches = yawMatches(0.025f, 0.070f, 1);
        break;
    case EnrollmentPose::YawLeft15:
        matches = yawMatches(0.025f, 0.070f, -1);
        break;
    case EnrollmentPose::YawRight30:
        matches = yawMatches(0.060f, 0.120f, 1);
        break;
    case EnrollmentPose::YawLeft30:
        matches = yawMatches(0.060f, 0.120f, -1);
        break;
    case EnrollmentPose::YawRight60:
        matches = yawMatches(0.110f, 0.190f, 1);
        break;
    case EnrollmentPose::YawLeft60:
        matches = yawMatches(0.110f, 0.190f, -1);
        break;
    case EnrollmentPose::SlightUp:
        matches = qAbs(nx) <= 0.060f && inRange(ny, -0.090f, -0.030f);
        break;
    case EnrollmentPose::SlightDown:
        matches = qAbs(nx) <= 0.060f && inRange(ny, 0.030f, 0.085f);
        break;
    }

    if (!matches && hint) {
        *hint = "Muovi poco la testa: frontale, 15, 30, massimo 60 gradi";
    }
    return matches;
}

void FaceRecognitionController::maybeCaptureEnrollmentFrame(const QImage& image) {
    if (!m_enrolling || m_enrollmentCapturedCount >= m_enrollmentTargetCount || image.isNull()) {
        return;
    }

    if (m_enrollmentTimer.isValid() && m_enrollmentTimer.elapsed() < 1500) {
        return;
    }

    if (m_enrollmentCapturedCount >= m_enrollmentPlan.size()) {
        return;
    }

    QString hint;
    if (!currentPoseMatches(m_enrollmentPlan[m_enrollmentCapturedCount].pose, &hint)) {
        setEnrollmentStatus(QString("%1/%2 - %3").arg(m_enrollmentCapturedCount).arg(m_enrollmentTargetCount).arg(hint));
        return;
    }

    QDir sessionDir(m_enrollmentTempDir);
    if (!sessionDir.exists()) {
        setEnrollmentStatus("Capture folder missing");
        m_enrolling = false;
        emit enrollmentChanged();
        return;
    }

    const int nextIndex = m_enrollmentCapturedCount + 1;
    const QString imagePath = sessionDir.filePath(QString("capture_%1.jpg").arg(nextIndex, 3, 10, QChar('0')));
    if (!image.save(imagePath, "JPG", 95)) {
        setEnrollmentStatus("Failed to save temp image");
        return;
    }

    m_enrollmentCapturedCount = nextIndex;
    m_enrollmentTimer.restart();
    setCaptureFlash(true);
    QTimer::singleShot(180, this, [this]() {
        setCaptureFlash(false);
    });

    if (m_enrollmentCapturedCount >= m_enrollmentTargetCount) {
        m_enrolling = false;
        setEnrollmentStatus("Running extractor");
        emit enrollmentChanged();
        runEnrollmentExtractor();
        return;
    }

    setEnrollmentStatus(QString("Capturing %1/%2 - %3")
                            .arg(m_enrollmentCapturedCount)
                            .arg(m_enrollmentTargetCount)
                            .arg(enrollmentInstruction()));
    emit enrollmentChanged();
}

void FaceRecognitionController::runEnrollmentExtractor() {
    runEnrollmentExtractor(m_enrollmentTempDir, m_enrollmentPersonName, true);
}

void FaceRecognitionController::runEnrollmentExtractor(const QString& inputDir,
                                                       const QString& personName,
                                                       bool cleanupInputDir) {
    if (m_extracting) {
        return;
    }

    const QString detectorPath = RuntimePaths::resolve("models/det_500m.onnx");
    const QString recognizerPath = RuntimePaths::resolve("models/w600k_mbf.onnx");
    const QString embeddingsDir = RuntimePaths::resolve("face_embeddings");
    QDir().mkpath(embeddingsDir);

    m_extracting = true;
    m_enrollmentCancelRequested = false;
    emit enrollmentChanged();

    auto result = std::make_shared<FaceEnrollmentResult>();

    const InferenceDevice inferenceDevice = m_inferenceDevice;
    QThread* thread = QThread::create([result, detectorPath, recognizerPath, embeddingsDir, inputDir, personName, inferenceDevice]() {
        qInfo().nospace() << "FaceEnrollmentExtractor: Requested " << inferenceDeviceName(inferenceDevice);
        FaceEnrollmentExtractor extractor(inferenceDevice);
        if (!extractor.initialize(detectorPath, recognizerPath)) {
            result->ok = false;
            result->message = "Failed to initialize C++ enrollment extractor";
            return;
        }
        *result = extractor.extract(inputDir, personName, embeddingsDir, true);
    });

    m_extractorThread = thread;
    connect(thread, &QThread::finished, this, [this, thread, result, embeddingsDir, cleanupInputDir]() {
        if (cleanupInputDir) {
            cleanupEnrollmentTempDir();
        }
        m_extracting = false;
        m_extractorThread = nullptr;

        if (m_enrollmentCancelRequested) {
            setEnrollmentStatus("Canceled");
        } else if (result->ok) {
            const bool reloadOk = !m_engine || !m_engine->isInitialized() || m_engine->reloadEmbeddings(embeddingsDir);
            setEnrollmentStatus(reloadOk ? result->message : "Face added, reload failed");
            refreshKnownFaces();
        } else {
            setEnrollmentStatus(result->message.isEmpty() ? "Extractor failed" : result->message.left(160));
        }

        m_enrollmentCapturedCount = 0;
        m_enrollmentTargetCount = 0;
        m_enrollmentPlan.clear();
        m_enrollmentCancelRequested = false;
        emit enrollmentChanged();
        thread->deleteLater();
    });

    thread->start();
}

void FaceRecognitionController::cleanupEnrollmentTempDir() {
    if (m_enrollmentTempDir.isEmpty()) {
        return;
    }

    QDir dir(m_enrollmentTempDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
    m_enrollmentTempDir.clear();
}

void FaceRecognitionController::setEnrollmentStatus(const QString& status) {
    if (m_enrollmentStatus == status) {
        return;
    }
    m_enrollmentStatus = status;
    emit enrollmentStatusChanged();
}

void FaceRecognitionController::setCaptureFlash(bool flash) {
    if (m_captureFlash == flash) {
        return;
    }
    m_captureFlash = flash;
    emit captureFlashChanged();
}

void FaceRecognitionController::setKnownFaces(const QStringList& faces) {
    if (m_knownFaces == faces) {
        return;
    }
    m_knownFaces = faces;
    emit knownFacesChanged();
}

void FaceRecognitionController::maybeSaveDetectedFaces(const RecognitionResult& result, const QImage& frame) {
    if (frame.isNull() || result.faces.empty()) {
        return;
    }

    int faceIndex = 0;
    for (const DetectedFace& face : result.faces) {
        const QString identity = autoSaveIdentityForFace(face);
        if (!identity.isEmpty()) {
            saveDetectedFaceImage(identity, face, frame, faceIndex);
        }
        ++faceIndex;
    }
}

bool FaceRecognitionController::saveDetectedFaceImage(const QString& identity,
                                                      const DetectedFace& face,
                                                      const QImage& frame,
                                                      int faceIndex) {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime lastSave = m_lastAutoSaveByIdentity.value(identity);
    if (lastSave.isValid() && lastSave.msecsTo(now) < AutoSaveMinimumIntervalMs) {
        return false;
    }

    const QRect imageRect(QPoint(0, 0), frame.size());
    QRectF faceRect(face.bbox.x, face.bbox.y, face.bbox.width, face.bbox.height);
    if (faceRect.width() <= 1.0 || faceRect.height() <= 1.0) {
        return false;
    }

    const qreal margin = qMax(faceRect.width(), faceRect.height()) * 0.18;
    faceRect = faceRect.adjusted(-margin, -margin, margin, margin);
    const QRect cropRect = faceRect.toAlignedRect().intersected(imageRect);
    if (cropRect.width() <= 1 || cropRect.height() <= 1) {
        return false;
    }

    const QString rootPath = RuntimePaths::resolve(QStringLiteral("faces"));
    QDir rootDir(rootPath);
    if (!rootDir.exists() && !rootDir.mkpath(QStringLiteral("."))) {
        qWarning() << "FaceRecognitionController: cannot create faces folder" << rootPath;
        return false;
    }

    const QString folderName = sanitizePersonName(identity);
    if (folderName.isEmpty()) {
        return false;
    }

    const QString folderPath = rootDir.filePath(folderName);
    QDir folderDir(folderPath);
    if (!folderDir.exists() && !rootDir.mkpath(folderName)) {
        qWarning() << "FaceRecognitionController: cannot create face folder" << folderPath;
        return false;
    }

    const QString timestamp = now.toLocalTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"));
    const QString fileName = QStringLiteral("face_%1_%2.jpg").arg(timestamp).arg(faceIndex, 2, 10, QChar('0'));
    const QString filePath = QDir(folderPath).filePath(fileName);
    const bool saved = frame.copy(cropRect).save(filePath, "JPG", 92);
    if (saved) {
        m_lastAutoSaveByIdentity.insert(identity, now);
    } else {
        qWarning() << "FaceRecognitionController: cannot save detected face" << filePath;
    }
    return saved;
}

QString FaceRecognitionController::autoSaveIdentityForFace(const DetectedFace& face) {
    if (face.embedding.empty()) {
        return {};
    }

    if (!isUnknownFaceName(face.name) && face.confidencePercent > FaceMatchMinimumConfidencePercent) {
        const QString safeName = sanitizePersonName(face.name);
        if (!safeName.isEmpty()) {
            return safeName;
        }
    }

    return {};
}

QString FaceRecognitionController::frameSource() const {
    if (m_frameRevision == 0) {
        return {};
    }
    return QString("image://frames/live?rev=%1").arg(m_frameRevision);
}

void FaceRecognitionController::setRunning(bool running) {
    if (m_running == running) {
        return;
    }
    m_running = running;
    emit runningChanged();
}

InferenceDevice FaceRecognitionController::sanitizeInferenceDevice(int device) {
    return device == 1 ? InferenceDevice::CUDA : InferenceDevice::CPU;
}

void FaceRecognitionController::recreateEngine() {
    m_workerBusy = false;
    m_engine = std::make_unique<FaceRecognitionEngine>(m_inferenceDevice, this);
    connect(m_engine.get(), &FaceRecognitionEngine::resultReady, this, &FaceRecognitionController::handleResult);
    connect(m_engine.get(), &FaceRecognitionEngine::landmarksReady, this, &FaceRecognitionController::landmarksReady);
    connect(m_engine.get(), &FaceRecognitionEngine::busyChanged, this, &FaceRecognitionController::handleBusyChanged);
    connect(m_engine.get(), &FaceRecognitionEngine::optionsChanged, this, [this]() {
        if (m_engine) {
            m_landmarkMode = static_cast<LandmarkMode>(m_engine->landmarkMode());
        }
        emit landmarkModeChanged();
    });
    m_engine->setLandmarkMode(static_cast<int>(m_landmarkMode));
}

cv::Mat FaceRecognitionController::imageToBgrMat(const QImage& image) {
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat rgbMat(rgb.height(),
                   rgb.width(),
                   CV_8UC3,
                   const_cast<uchar*>(rgb.bits()),
                   static_cast<size_t>(rgb.bytesPerLine()));
    cv::Mat bgr;
    cv::cvtColor(rgbMat, bgr, cv::COLOR_RGB2BGR);
    return bgr.clone();
}
