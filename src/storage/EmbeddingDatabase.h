#pragma once

#include <QString>
#include <QJsonObject>
#include <vector>

/**
 * @brief Volto noto caricato dal database embedding.
 */
struct KnownFace {
    /** Nome associato all'embedding. */
    QString name;
    /** Embedding facciale salvato su disco. */
    std::vector<float> embedding;
};

/**
 * @brief Database in memoria degli embedding facciali noti.
 *
 * Carica gli embedding dalla cartella face_embeddings e permette di cercare il
 * volto piu simile a un embedding prodotto dal recognizer.
 */
class EmbeddingDatabase {
public:
    /**
     * @brief Carica gli embedding da una cartella.
     * @param dirPath Cartella contenente embeddings.json o file JSON singoli.
     * @return true se almeno un volto valido e stato caricato.
     *
     * Formato preferito: embeddings.json con array "faces".
     * Fallback: un file .json per volto.
     */
    bool load(const QString& dirPath);

    /**
     * @brief Cerca il volto noto piu simile all'embedding in input.
     * @param embedding Embedding da confrontare.
     * @param threshold Soglia minima di cosine similarity per accettare il match.
     *                  0.60 corrisponde a una confidence strettamente maggiore dell'80%.
     * @return Coppia nome/confidence. Il nome e "Unknown" se non supera soglia.
     */
    std::pair<QString, float> match(const std::vector<float>& embedding, float threshold = 0.60f) const;

    /**
     * @brief Indica se non sono presenti volti noti in memoria.
     */
    bool empty() const { return m_faces.empty(); }

private:
    std::vector<KnownFace> m_faces;

    /**
     * @brief Estrae il vettore embedding da un oggetto JSON.
     * @param obj Oggetto che contiene il campo array "embedding".
     * @return Vettore float; vuoto se il campo manca o e vuoto.
     */
    static std::vector<float> parseEmbedding(const QJsonObject& obj);
};
