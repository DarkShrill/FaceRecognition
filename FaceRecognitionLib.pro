QT += core
QT -= gui

TEMPLATE = lib
TARGET = FaceRecognition

CONFIG += dll
CONFIG -= app_bundle

DEFINES += FACERECOGNITION_LIBRARY

include(FaceRecognitionBackend.pri)

CONFIG(debug, debug|release) {
    DESTDIR = $$OUT_PWD/debug
} else {
    DESTDIR = $$OUT_PWD/release
}

win32 {
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
