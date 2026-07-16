#include "FaceEnrollmentExtractor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

#include <algorithm>

FaceEnrollmentExtractor::FaceEnrollmentExtractor(InferenceDevice device)
    : m_detector(device), m_recognizer(device) {}

bool FaceEnrollmentExtractor::initialize(const QString& detectorModel, const QString& recognizerModel) {
    const bool detectorReady = m_detector.load(detectorModel, cv::Size(640, 640));
    const bool recognizerReady = m_recognizer.load(recognizerModel);
    m_ready = detectorReady && recognizerReady;
    return m_ready;
}

FaceEnrollmentResult FaceEnrollmentExtractor::extract(const QString& inputDir,
                                                      const QString& personName,
                                                      const QString& saveDir,
                                                      bool replaceExisting) {
    FaceEnrollmentResult result;
    if (!m_ready) {
        result.message = "Enrollment extractor is not initialized";
        return result;
    }

    QDir input(inputDir);
    if (!input.exists()) {
        result.message = "Input folder not found";
        return result;
    }

    const QString trimmedName = personName.trimmed();
    if (trimmedName.isEmpty()) {
        result.message = "Person name is empty";
        return result;
    }

    const QStringList files = input.entryList({"*.jpg", "*.jpeg", "*.png", "*.bmp"}, QDir::Files, QDir::Name);
    std::vector<std::vector<float>> embeddings;
    embeddings.reserve(files.size());

    for (const QString& fileName : files) {
        const QString imagePath = input.filePath(fileName);
        const cv::Mat image = cv::imread(imagePath.toStdString(), cv::IMREAD_COLOR);
        //cv::cvtColor(image, image, cv::COLOR_BGR2RGB);

        if (image.empty()) {
            ++result.skippedImages;
            continue;
        }

        bool exactlyOneFace = false;
        std::vector<float> embedding = extractOne(image, &exactlyOneFace);
        if (exactlyOneFace && !embedding.empty()) {
            embeddings.push_back(std::move(embedding));
            ++result.validImages;
        } else {
            ++result.skippedImages;
        }
    }

    if (embeddings.empty()) {
        result.message = "No valid images with exactly one face";
        return result;
    }

    std::vector<float> average(embeddings.front().size(), 0.0f);
    for (const auto& embedding : embeddings) {
        if (embedding.size() != average.size()) {
            ++result.skippedImages;
            continue;
        }
        for (size_t i = 0; i < average.size(); ++i) {
            average[i] += embedding[i];
        }
    }
    for (float& value : average) {
        value /= static_cast<float>(embeddings.size());
    }

    QDir output(saveDir);
    if (!output.exists() && !output.mkpath(".")) {
        result.message = "Cannot create face_embeddings folder";
        return result;
    }

    QString error;
    const QString pklPath = output.filePath(QString("%1.pkl").arg(safeFileStem(trimmedName)));
    if (!writePickleFloatList(pklPath, average, &error)) {
        result.message = error;
        return result;
    }
    if (!updateEmbeddingsJson(saveDir, trimmedName, average, replaceExisting, &error)) {
        result.message = error;
        return result;
    }
    if (!updateMetadata(saveDir, trimmedName, &error)) {
        result.message = error;
        return result;
    }

    result.ok = true;
    result.message = QString("Stored %1 valid images for %2").arg(result.validImages).arg(trimmedName);
    return result;
}

std::vector<float> FaceEnrollmentExtractor::extractOne(const cv::Mat& image, bool* exactlyOneFace) const {
    if (exactlyOneFace) {
        *exactlyOneFace = false;
    }

    cv::Mat smallFrame;
    constexpr double scaleFactor = 0.5;
    cv::resize(image, smallFrame, cv::Size(), scaleFactor, scaleFactor);

    auto faces = m_detector.detect(smallFrame, 0, "default");
    if (faces.size() != 1) {
        return {};
    }

    DetectedFace face = faces.front();
    face.bbox.x /= static_cast<float>(scaleFactor);
    face.bbox.y /= static_cast<float>(scaleFactor);
    face.bbox.width /= static_cast<float>(scaleFactor);
    face.bbox.height /= static_cast<float>(scaleFactor);
    for (auto& point : face.kps) {
        point.x /= static_cast<float>(scaleFactor);
        point.y /= static_cast<float>(scaleFactor);
    }

    const cv::Mat aligned = m_aligner.normCrop(image, face.kps, 112);
    std::vector<float> embedding = m_recognizer.extract(aligned);
    if (exactlyOneFace) {
        *exactlyOneFace = true;
    }
    return embedding;
}

QString FaceEnrollmentExtractor::safeFileStem(const QString& personName) {
    QString safe = personName.trimmed();
    safe.replace(QRegularExpression("\\s+"), "_");
    safe.replace(QRegularExpression("[^A-Za-z0-9_\\-]"), "");
    return safe.left(80);
}

bool FaceEnrollmentExtractor::writePickleFloatList(const QString& filePath,
                                                   const std::vector<float>& embedding,
                                                   QString* error) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error) {
            *error = "Cannot write pkl file";
        }
        return false;
    }

    QTextStream out(&file);
    out.setRealNumberNotation(QTextStream::FixedNotation);
    out.setRealNumberPrecision(9);
    out << "(lp0\n";
    for (float value : embedding) {
        out << "F" << value << "\na";
    }
    out << ".\n";
    return true;
}

bool FaceEnrollmentExtractor::updateMetadata(const QString& saveDir, const QString& personName, QString* error) {
    QDir dir(saveDir);
    const QString metadataPath = dir.filePath("metadata.json");
    QSet<QString> names;

    QFile input(metadataPath);
    if (input.exists() && input.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(input.readAll());
        for (const QJsonValue& value : doc.object().value("names").toArray()) {
            const QString name = value.toString().trimmed();
            if (!name.isEmpty()) {
                names.insert(name);
            }
        }
    }

    names.insert(personName);

    QJsonArray array;
    QList<QString> sorted = names.values();
    std::sort(sorted.begin(), sorted.end());
    for (const QString& name : sorted) {
        array.append(name);
    }

    QFile output(metadataPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = "Cannot write metadata.json";
        }
        return false;
    }
    output.write(QJsonDocument(QJsonObject{{"names", array}}).toJson(QJsonDocument::Indented));
    return true;
}

bool FaceEnrollmentExtractor::updateEmbeddingsJson(const QString& saveDir,
                                                   const QString& personName,
                                                   const std::vector<float>& embedding,
                                                   bool replaceExisting,
                                                   QString* error) {
    QDir dir(saveDir);
    const QString embeddingsPath = dir.filePath("embeddings.json");
    QJsonArray faces;

    QFile input(embeddingsPath);
    if (input.exists() && input.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(input.readAll());
        for (const QJsonValue& value : doc.object().value("faces").toArray()) {
            if (!value.isObject()) {
                continue;
            }
            const QJsonObject object = value.toObject();
            if (object.value("name").toString() == personName) {
                if (!replaceExisting) {
                    if (error) {
                        *error = "Person already exists";
                    }
                    return false;
                }
                continue;
            }
            faces.append(object);
        }
    }

    QJsonArray embeddingArray;
    for (float value : embedding) {
        embeddingArray.append(static_cast<double>(value));
    }
    faces.append(QJsonObject{{"name", personName}, {"embedding", embeddingArray}});

    QFile output(embeddingsPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = "Cannot write embeddings.json";
        }
        return false;
    }
    output.write(QJsonDocument(QJsonObject{{"faces", faces}}).toJson(QJsonDocument::Indented));
    return true;
}
