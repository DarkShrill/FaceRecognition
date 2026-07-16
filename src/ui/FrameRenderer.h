#pragma once

#include "FaceTypes.h"

#include <QColor>
#include <QImage>
#include <QSize>

class FrameRenderer {
public:
    QImage renderOverlay(const QSize& size,
                         const RecognitionResult& result,
                         LandmarkMode landmarkMode) const;

private:
    static QColor colorForFace(const DetectedFace& face);
};
