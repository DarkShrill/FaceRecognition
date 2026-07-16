#pragma once

#include <vector>

/**
 * @brief Funzioni matematiche sugli embedding facciali.
 *
 * Il namespace evita di accoppiare il database degli embedding al recognizer
 * ONNX: il database deve solo confrontare vettori, non conoscere il modello.
 */
namespace EmbeddingMath {

/**
 * @brief Calcola la cosine similarity tra due embedding.
 * @param a Primo embedding.
 * @param b Secondo embedding.
 * @return Valore in genere compreso tra -1 e 1. Ritorna -1 se i vettori sono
 *         vuoti, hanno dimensioni diverse o hanno norma nulla.
 */
float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);

}
