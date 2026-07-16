QT += core gui qml quick

CONFIG -= app_bundle
CONFIG -= qtquickcompiler

DEFINES += FACERECOGNITION_STATIC

TEMPLATE = app
TARGET = FaceRecognition

CONFIG(debug, debug|release) {
    DESTDIR = $$OUT_PWD/debug
    QVIDEOSTREAM_CONFIG_DIR = debug
} else {
    DESTDIR = $$OUT_PWD/release
    QVIDEOSTREAM_CONFIG_DIR = release
}

include(FaceRecognitionBackend.pri)

QVIDEOSTREAM_ROOT = $$PWD/third_party/qvideostream
QVIDEOSTREAM_LIB_DIR = $$OUT_PWD/third_party/qvideostream/$$QVIDEOSTREAM_CONFIG_DIR
QVIDEOSTREAM_DLL = $$QVIDEOSTREAM_LIB_DIR/QVideoStream.dll
QVIDEOSTREAM_FFMPEG_BIN = $$QVIDEOSTREAM_ROOT/ffmpeg/bin

INCLUDEPATH += \
    $$QVIDEOSTREAM_ROOT \
    src/api \
    src/ui \
    src/app

LIBS += \
    -L$$QVIDEOSTREAM_LIB_DIR -lQVideoStream

win32 {
    PRE_TARGETDEPS += $$QVIDEOSTREAM_LIB_DIR/QVideoStream.lib
    QVIDEOSTREAM_DLL_WIN = $$replace(QVIDEOSTREAM_DLL, /, \\)
    QVIDEOSTREAM_FFMPEG_DLLS_WIN = $$replace(QVIDEOSTREAM_FFMPEG_BIN, /, \\)\\*.dll
    DESTDIR_WIN = $$replace(DESTDIR, /, \\)
}

SOURCES += \
    src/app/main.cpp \
    src/ui/FaceRecognitionController.cpp \
    src/ui/FrameRenderer.cpp

HEADERS += \
    src/ui/FaceRecognitionController.h \
    src/ui/FrameImageProvider.h \
    src/ui/FrameRenderer.h

RESOURCES += qml.qrc

win32 {
    QMAKE_POST_LINK += $$quote(cmd /c copy /Y \"$$QVIDEOSTREAM_DLL_WIN\" \"$$DESTDIR_WIN\\\" $$escape_expand(\\n\\t))
    QMAKE_POST_LINK += $$quote(cmd /c copy /Y \"$$QVIDEOSTREAM_FFMPEG_DLLS_WIN\" \"$$DESTDIR_WIN\\\" $$escape_expand(\\n\\t))
    QMAKE_POST_LINK += $$quote(cmd /c if not exist \"$$DESTDIR\\models\" mkdir \"$$DESTDIR\\models\" $$escape_expand(\\n\\t))
    QMAKE_POST_LINK += $$quote(cmd /c xcopy /Y /I \"$$PWD\\models\\*.onnx\" \"$$DESTDIR\\models\\\" $$escape_expand(\\n\\t))
    QMAKE_POST_LINK += $$quote(cmd /c if not exist \"$$DESTDIR\\face_embeddings\" mkdir \"$$DESTDIR\\face_embeddings\" $$escape_expand(\\n\\t))
    QMAKE_POST_LINK += $$quote(cmd /c xcopy /Y /I \"$$PWD\\face_embeddings\\*\" \"$$DESTDIR\\face_embeddings\\\" $$escape_expand(\\n\\t))
    QMAKE_POST_LINK += $$quote(cmd /c copy /Y \"$$ONNXRUNTIME_ROOT\\lib\\onnxruntime.dll\" \"$$DESTDIR\\\" $$escape_expand(\\n\\t))
    QMAKE_POST_LINK += $$quote(cmd /c copy /Y \"$$ONNXRUNTIME_ROOT\\lib\\onnxruntime_providers_shared.dll\" \"$$DESTDIR\\\" $$escape_expand(\\n\\t))
    QMAKE_POST_LINK += $$quote(cmd /c if exist \"$$ONNXRUNTIME_ROOT\\lib\\onnxruntime_providers_cuda.dll\" copy /Y \"$$ONNXRUNTIME_ROOT\\lib\\onnxruntime_providers_cuda.dll\" \"$$DESTDIR\\\" $$escape_expand(\\n\\t))
    QMAKE_POST_LINK += $$quote(cmd /c copy /Y \"$$OPENCV_ROOT\\x64\\vc17\\bin\\opencv_world4130.dll\" \"$$DESTDIR\\\" $$escape_expand(\\n\\t))
    # QMAKE_POST_LINK += $$quote(cmd /c if exist \"$$OPENCV_ROOT\\x64\\vc17\\bin\\opencv_videoio_ffmpeg4130_64.dll\" copy /Y \"$$OPENCV_ROOT\\x64\\vc17\\bin\\opencv_videoio_ffmpeg4130_64.dll\" \"$$DESTDIR\\\" $$escape_expand(\\n\\t))

    ONNXRUNTIME_DLL = $$ONNXRUNTIME_ROOT/lib/onnxruntime.dll
    ONNXRUNTIME_SHARED_DLL = $$ONNXRUNTIME_ROOT/lib/onnxruntime_providers_shared.dll
    ONNXRUNTIME_CUDA_DLL = $$ONNXRUNTIME_ROOT/lib/onnxruntime_providers_cuda.dll
    OPENCV_DLL = $$OPENCV_ROOT/x64/vc17/bin/opencv_world4130.dll

    DESTDIR_WIN = $$replace(DESTDIR, /, \\)
    ONNXRUNTIME_DLL_WIN = $$replace(ONNXRUNTIME_DLL, /, \\)
    ONNXRUNTIME_SHARED_DLL_WIN = $$replace(ONNXRUNTIME_SHARED_DLL, /, \\)
    ONNXRUNTIME_CUDA_DLL_WIN = $$replace(ONNXRUNTIME_CUDA_DLL, /, \\)
    OPENCV_DLL_WIN = $$replace(OPENCV_DLL, /, \\)

    QMAKE_POST_LINK += $$quote(cmd /c if exist \"$$ONNXRUNTIME_DLL_WIN\" copy /Y \"$$ONNXRUNTIME_DLL_WIN\" \"$$DESTDIR_WIN\\\" $$escape_expand(\\n\\t))
    QMAKE_POST_LINK += $$quote(cmd /c if exist \"$$ONNXRUNTIME_SHARED_DLL_WIN\" copy /Y \"$$ONNXRUNTIME_SHARED_DLL_WIN\" \"$$DESTDIR_WIN\\\" $$escape_expand(\\n\\t))
    QMAKE_POST_LINK += $$quote(cmd /c if exist \"$$ONNXRUNTIME_CUDA_DLL_WIN\" copy /Y \"$$ONNXRUNTIME_CUDA_DLL_WIN\" \"$$DESTDIR_WIN\\\" $$escape_expand(\\n\\t))
    QMAKE_POST_LINK += $$quote(cmd /c if exist \"$$OPENCV_DLL_WIN\" copy /Y \"$$OPENCV_DLL_WIN\" \"$$DESTDIR_WIN\\\" $$escape_expand(\\n\\t))
}
