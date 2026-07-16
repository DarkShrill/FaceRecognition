CONFIG += c++17
CONFIG += no_mocdepend
CONFIG -= depend_includepath

isEmpty(ONNXRUNTIME_ROOT) {
    ONNXRUNTIME_ROOT = C:/onnxruntime
}

isEmpty(OPENCV_ROOT) {
    OPENCV_ROOT = C:/opencv-4.13.0/build/install
}

DEFINES += APP_SOURCE_DIR=\\\"$$PWD\\\"

INCLUDEPATH += \
    $$ONNXRUNTIME_ROOT/include \
    $$OPENCV_ROOT/include \
    src/api \
    src/inference \
    src/pipeline \
    src/storage \
    src/utils \
    src/vision

LIBS += -L$$OPENCV_ROOT/x64/vc17/lib -lopencv_world4130

LIBS += \
    -L$$ONNXRUNTIME_ROOT/lib -lonnxruntime

exists($$ONNXRUNTIME_ROOT/lib/onnxruntime_providers_cuda.lib) {
    DEFINES += USE_CUDA_PROVIDER
    LIBS += -L$$ONNXRUNTIME_ROOT/lib -lonnxruntime_providers_shared -lonnxruntime_providers_cuda
    message("ONNX Runtime CUDA provider enabled")
} else {
    message("ONNX Runtime CUDA provider not found; building with CPU fallback only")
}

SOURCES += \
    src/api/FaceRecognitionEngine.cpp \
    src/pipeline/FaceEnrollmentExtractor.cpp \
    src/pipeline/FaceRecognitionPipeline.cpp \
    src/pipeline/FaceRecognitionWorker.cpp \
    src/storage/EmbeddingDatabase.cpp \
    src/utils/RuntimePaths.cpp \
    src/vision/EmbeddingMath.cpp \
    src/vision/FaceAligner.cpp \
    src/vision/FaceDetector.cpp \
    src/vision/FaceLandmarker.cpp \
    src/vision/FaceLandmarker3D68.cpp \
    src/vision/FaceRecognizer.cpp

HEADERS += \
    src/api/FaceRecognitionEngine.h \
    src/api/FaceRecognitionGlobal.h \
    src/inference/InferenceDevice.h \
    src/pipeline/FaceEnrollmentExtractor.h \
    src/pipeline/FaceRecognitionPipeline.h \
    src/pipeline/FaceRecognitionWorker.h \
    src/storage/EmbeddingDatabase.h \
    src/utils/RuntimePaths.h \
    src/vision/EmbeddingMath.h \
    src/vision/FaceAligner.h \
    src/vision/FaceDetector.h \
    src/vision/FaceLandmarker.h \
    src/vision/FaceLandmarker3D68.h \
    src/vision/FaceRecognizer.h \
    src/vision/FaceTypes.h
