#pragma once

#include <opencv2/opencv.hpp>
#include <array>

/**
 * @brief Allinea un volto usando i 5 landmark facciali standard.
 *
 * Trasforma il volto rilevato in un crop normalizzato, compatibile con
 * l'input atteso dal modello di riconoscimento.
 */
class FaceAligner {
public:
    /**
     * @brief Produce un crop allineato del volto.
     * @param image Frame originale BGR.
     * @param srcPts Landmark sorgente del volto rilevato.
     * @param imageSize Dimensione quadrata del crop finale, tipicamente 112.
     * @return Crop allineato. Ritorna cv::Mat vuoto se la trasformazione fallisce.
     */
    cv::Mat normCrop(const cv::Mat& image,
                     const std::array<cv::Point2f, 5>& srcPts,
                     int imageSize = 112) const;

private:
    /**
     * @brief Calcola i landmark target per la dimensione richiesta.
     * @param imageSize Dimensione del crop finale.
     * @return Cinque punti target usati per stimare la trasformazione affine.
     */
    std::array<cv::Point2f, 5> targetPoints(int imageSize) const;
};
