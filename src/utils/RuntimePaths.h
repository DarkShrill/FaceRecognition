#pragma once

#include "FaceRecognitionGlobal.h"

#include <QString>

/**
 * @brief Utility centralizzata per trovare file e cartelle necessari a runtime.
 *
 * La risoluzione prova prima la cartella dell'eseguibile, poi la working
 * directory e infine la cartella sorgente definita da APP_SOURCE_DIR. Questo
 * permette di eseguire l'app sia da build output sia dall'ambiente di sviluppo.
 */
class FACERECOGNITION_EXPORT RuntimePaths {
public:
    /**
     * @brief Risolve un path relativo in un path assoluto esistente, se possibile.
     * @param relativePath Path relativo, per esempio "models/det_500m.onnx".
     * @return Il primo path trovato. Se nessun file esiste, ritorna comunque
     *         il path relativo alla cartella dell'eseguibile.
     */
    static QString resolve(const QString& relativePath);
};
