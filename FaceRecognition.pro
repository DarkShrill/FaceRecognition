TEMPLATE = subdirs
CONFIG += ordered

FACE_RECOGNITION_MODE = gui #gui

contains(CONFIG, facerecognition_dll) {
    FACE_RECOGNITION_MODE = dll
}

contains(CONFIG, facerecognition_lib) {
    FACE_RECOGNITION_MODE = dll
}

contains(CONFIG, facerecognition_gui) {
    FACE_RECOGNITION_MODE = gui
}

equals(FACE_RECOGNITION_MODE, dll) {
    message("FaceRecognition build mode: DLL")

    SUBDIRS += facerecognitionlib
    facerecognitionlib.file = FaceRecognitionLib.pro
} else {
    message("FaceRecognition build mode: GUI")

    SUBDIRS += \
        qvideostream \
        facerecognitionapp

    qvideostream.file = third_party/qvideostream/QVideoStream.pro

    facerecognitionapp.file = FaceRecognitionApp.pro
    facerecognitionapp.depends = qvideostream
}
