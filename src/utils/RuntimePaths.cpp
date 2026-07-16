#include "RuntimePaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

QString RuntimePaths::resolve(const QString& relativePath) {
    const QString appPath = QDir(QCoreApplication::applicationDirPath()).filePath(relativePath);
    if (QFileInfo::exists(appPath)) {
        return appPath;
    }

    const QString cwdPath = QDir(QDir::currentPath()).filePath(relativePath);
    if (QFileInfo::exists(cwdPath)) {
        return cwdPath;
    }

#ifdef APP_SOURCE_DIR
    const QString sourcePath = QDir(QStringLiteral(APP_SOURCE_DIR)).filePath(relativePath);
    if (QFileInfo::exists(sourcePath)) {
        return sourcePath;
    }
#endif

    return appPath;
}
