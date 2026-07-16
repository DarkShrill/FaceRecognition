#ifndef FRAMEIMAGEPROVIDER_H
#define FRAMEIMAGEPROVIDER_H

#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QQuickImageProvider>
#include <Qt>


class FrameImageProvider : public QQuickImageProvider {
public:
    FrameImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString&, QSize* size, const QSize&) override {
        QMutexLocker locker(&m_mutex);
        if (m_image.isNull()) {
            QImage transparent(1, 1, QImage::Format_ARGB32_Premultiplied);
            transparent.fill(Qt::transparent);
            if (size) {
                *size = transparent.size();
            }
            return transparent;
        }

        if (size) {
            *size = m_image.size();
        }
        return m_image;
    }

    void setImage(const QImage& image) {
        QMutexLocker locker(&m_mutex);
        m_image = image;
    }

private:
    QImage m_image;
    QMutex m_mutex;
};

#endif // FRAMEIMAGEPROVIDER_H
