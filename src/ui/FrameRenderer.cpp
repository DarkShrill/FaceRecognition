#include "FrameRenderer.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QtMath>

QImage FrameRenderer::renderOverlay(const QSize& size,
                                    const RecognitionResult& result,
                                    LandmarkMode landmarkMode) const {
    if (!size.isValid() || size.isEmpty()) {
        return {};
    }

    QImage overlay(size, QImage::Format_ARGB32_Premultiplied);
    overlay.fill(Qt::transparent);

    QPainter painter(&overlay);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QFont labelFont = painter.font();
    labelFont.setPixelSize(qMax(13, size.height() / 42));
    labelFont.setBold(true);
    painter.setFont(labelFont);

    const qreal pointRadius = qMax<qreal>(2.0, size.height() / 240.0);
    const bool drawFivePoints = shouldExposeFivePointLandmarks(landmarkMode);
    const bool draw106Points = shouldExpose106Landmarks(landmarkMode);
    const bool draw3D68Points = shouldExpose3D68Landmarks(landmarkMode);

    for (const DetectedFace& face : result.faces) {
        const QColor faceColor = colorForFace(face);
        const QRectF box(face.bbox.x, face.bbox.y, face.bbox.width, face.bbox.height);

        painter.setPen(QPen(faceColor, 3.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(box, 4.0, 4.0);

        const QString label = face.name.isEmpty()
                                  ? QStringLiteral("Unknown")
                                  : QStringLiteral("%1 %2%")
                                        .arg(face.name)
                                        .arg(face.confidencePercent, 0, 'f', 1);
        const QRectF labelBounds = painter.fontMetrics().boundingRect(label).adjusted(-8, -4, 8, 4);
        QRectF labelRect(box.left(), box.top() - labelBounds.height() - 4.0, labelBounds.width(), labelBounds.height());
        if (labelRect.top() < 0.0) {
            labelRect.moveTop(box.top() + 4.0);
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 170));
        painter.drawRoundedRect(labelRect, 4.0, 4.0);
        painter.setPen(Qt::white);
        painter.drawText(labelRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, label);

        if (draw106Points) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 210, 255, 210));
            for (const cv::Point2f& point : face.landmarks106) {
                painter.drawEllipse(QPointF(point.x, point.y), pointRadius, pointRadius);
            }
        }

        if (draw3D68Points) {
            painter.setPen(QPen(QColor(255, 70, 210, 230), 1.0));
            painter.setBrush(QColor(255, 70, 210, 190));
            for (const cv::Point3f& point : face.landmarks3d68) {
                painter.drawEllipse(QPointF(point.x, point.y), pointRadius * 1.25, pointRadius * 1.25);
            }
        }

        if (drawFivePoints) {
            painter.setPen(QPen(QColor(20, 20, 20, 190), 1.0));
            painter.setBrush(QColor(255, 210, 70, 230));
            for (const cv::Point2f& point : face.kps) {
                painter.drawEllipse(QPointF(point.x, point.y), pointRadius * 1.6, pointRadius * 1.6);
            }
        }
    }

    return overlay;
}

QColor FrameRenderer::colorForFace(const DetectedFace& face) {
    if (face.name.isEmpty() || face.name.compare(QStringLiteral("Unknown"), Qt::CaseInsensitive) == 0) {
        return QColor(255, 180, 80);
    }
    return QColor(70, 220, 130);
}
