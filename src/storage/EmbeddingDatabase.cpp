#include "EmbeddingDatabase.h"
#include "EmbeddingMath.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

std::vector<float> EmbeddingDatabase::parseEmbedding(const QJsonObject& obj)
{
    std::vector<float> emb;
    QJsonArray arr = obj.value("embedding").toArray();
    emb.reserve(arr.size());

    for (const auto& v : arr) {
        emb.push_back(static_cast<float>(v.toDouble()));
    }

    return emb;
}

bool EmbeddingDatabase::load(const QString& dirPath)
{
    m_faces.clear();

    QDir dir(dirPath);
    if (!dir.exists()) {
        qWarning() << "EmbeddingDatabase: directory does not exist:" << dirPath;
        return false;
    }

    // embeddings.json e la sorgente autorevole quando esiste.
    const QString mergedFilePath = dir.filePath("embeddings.json");
    if (QFile::exists(mergedFilePath)) {
        QFile f(mergedFilePath);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);

            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject root = doc.object();
                QJsonArray facesArray = root.value("faces").toArray();

                for (const auto& item : facesArray) {
                    if (!item.isObject()) {
                        continue;
                    }

                    QJsonObject obj = item.toObject();
                    QString name = obj.value("name").toString().trimmed();
                    auto emb = parseEmbedding(obj);

                    if (!name.isEmpty() && !emb.empty()) {
                        m_faces.push_back({name, emb});
                    }
                }

                if (!m_faces.empty()) {
                    qDebug() << "EmbeddingDatabase: loaded" << m_faces.size()
                    << "faces from" << mergedFilePath;
                    return true;
                }

                qWarning() << "EmbeddingDatabase: embeddings.json contains no valid faces:" << mergedFilePath;
                return false;
            } else {
                qWarning() << "EmbeddingDatabase: invalid embeddings.json:"
                           << err.errorString();
                return false;
            }
        } else {
            qWarning() << "EmbeddingDatabase: cannot open" << mergedFilePath;
            return false;
        }
    }

    // Fallback solo se embeddings.json non esiste: vecchio formato, un file .json per faccia.
    const QStringList files = dir.entryList({"*.json"}, QDir::Files);

    for (const QString& fileName : files) {
        if (fileName.compare("embeddings.json", Qt::CaseInsensitive) == 0) {
            continue;
        }

        QFile f(dir.filePath(fileName));
        if (!f.open(QIODevice::ReadOnly)) {
            continue;
        }

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            continue;
        }

        QJsonObject obj = doc.object();

        QString defaultName = QFileInfo(fileName).completeBaseName();
        QString name = obj.value("name").toString(defaultName).trimmed();
        auto emb = parseEmbedding(obj);

        if (!name.isEmpty() && !emb.empty()) {
            m_faces.push_back({name, emb});
        }
    }

    if (!m_faces.empty()) {
        qDebug() << "EmbeddingDatabase: loaded" << m_faces.size()
        << "faces from per-file jsons in" << dirPath;
        return true;
    }

    qWarning() << "EmbeddingDatabase: no embeddings loaded from" << dirPath;
    return false;
}

std::pair<QString, float> EmbeddingDatabase::match(const std::vector<float>& embedding,
                                                   float threshold) const
{
    if (embedding.empty() || m_faces.empty()) {
        return {"Unknown", 0.0f};
    }

    QString bestName = "Unknown";
    float bestScore = -1.0f;

    for (const auto& face : m_faces) {
        float sim = EmbeddingMath::cosineSimilarity(embedding, face.embedding);
        if (sim > bestScore) {
            bestScore = sim;
            bestName = face.name;
        }
    }

    // score cosine tipicamente in [-1, 1]
    float confidence = (bestScore + 1.0f) * 50.0f;

    if (bestScore <= threshold) {
        return {"Unknown", confidence};
    }

    return {bestName, confidence};
}
