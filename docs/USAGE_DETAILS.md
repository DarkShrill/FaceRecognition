# FaceRecognition - Runtime Flow, DLL API, Build, And Dependencies

This page contains the extended documentation that follows the class wiring and
signal/slot section in the main README.

## Runtime Flow

### GUI Startup And Live Analysis

```mermaid
sequenceDiagram
    participant User
    participant QML as qml/Main.qml
    participant C as FaceRecognitionController
    participant E as FaceRecognitionEngine
    participant W as FaceRecognitionWorker
    participant P as FaceRecognitionPipeline
    participant R as FrameRenderer
    participant IP as FrameImageProvider

    User->>QML: presses Start
    QML->>C: start()
    C->>E: initialize(detector, landmark, 3d, recognizer, embeddings)
    E->>W: initialize(...) in the worker thread
    W->>P: loads ONNX models and database
    P-->>W: ok
    W-->>E: ok
    E-->>C: initializedChanged
    C-->>QML: runningChanged + statusChanged

    loop each useful video frame
        QML->>C: submitVideoFrame(QImage)
        C->>C: converts RGB QImage to BGR cv::Mat
        C->>E: submitFrame(frame)
        E->>W: processFrame(frame)
        W->>P: process(frame)
        P-->>W: RecognitionResult
        W-->>E: resultReady(result)
        E-->>C: resultReady(result)
        C->>R: renderOverlay(frameSize, result, landmarkMode)
        R-->>C: overlay QImage
        C->>IP: setImage(overlay)
        C-->>QML: frameSourceChanged + metricsChanged
        QML->>IP: reads image://frames/live?rev=N
    end
```

### Single-Frame Pipeline

```mermaid
flowchart TD
    A["BGR frame"] --> B["brightness calculation"]
    B --> C["resize 0.5x for detection"]
    C --> D["FaceDetector.detect"]
    D --> E["map bbox and 5 keypoints back to original size"]
    E --> F{"does landmarkMode require 106 points?"}
    F -->|"yes"| G["FaceLandmarker.detect"]
    F -->|"no"| H["skip 106"]
    G --> I{"does landmarkMode require 3D 68 points?"}
    H --> I
    I -->|"yes"| J["FaceLandmarker3D68.detect"]
    I -->|"no"| K["skip 3D 68"]
    J --> L["FaceAligner.normCrop 112x112"]
    K --> L
    L --> M["FaceRecognizer.extract"]
    M --> N["EmbeddingDatabase.match"]
    N --> O["DetectedFace with name, similarity, confidence"]
    O --> P["RecognitionResult with faces, fps, brightness, lowLight"]
```

### Add Face Enrollment

```mermaid
flowchart TD
    A["User enters Person and presses Capture"] --> B["startFaceEnrollment"]
    B --> C["create tmp/add_faces/<name_timestamp>"]
    C --> D["15 guided poses"]
    D --> E["check face and pose on each useful frame"]
    E -->|"valid pose"| F["save capture_XXX.jpg"]
    E -->|"invalid pose"| G["show hint in the GUI"]
    F --> H{"15 photos acquired?"}
    H -->|"no"| E
    H -->|"yes"| I["FaceEnrollmentExtractor on QThread"]
    I --> J["detector + aligner + recognizer"]
    J --> K["average embedding"]
    K --> L["update embeddings.json, metadata.json, .pkl"]
    L --> M["refreshKnownFaces + reloadEmbeddings"]
```

## Reusable API And DLL

The library entry point is `FaceRecognitionEngine`.

The DLL is identical to the GUI in the backend part: it uses the same sources
from `FaceRecognitionBackend.pri`, the same models, the same pipeline, the same
types (`RecognitionResult`, `DetectedFace`, `FaceRecognitionOptions`), and the
same main signals. The difference is that the DLL has no graphics, QML,
`FaceRecognitionController`, `FrameRenderer`, `FrameImageProvider`, or
QVideoStream. A DLL user must therefore:

1. create a Qt app with `QCoreApplication` or `QApplication`;
2. create `FaceRecognitionEngine`;
3. call `initialize(...)` with model paths and the embedding folder;
4. send BGR OpenCV frames with `submitFrame(cv::Mat)`;
5. listen to `resultReady(RecognitionResult)` and, if separate landmarks are
   needed, `landmarksReady(RecognitionResult)`;
6. decide outside the DLL how to capture video, draw overlays, save logs, or
   update a GUI.

`landmarkMode` can also be changed at runtime:

- `0`: no full landmarks exposed;
- `1`: only the detector's 5 keypoints;
- `2`: only 106 landmarks;
- `3`: all available landmarks;
- `4`: only 68 3D landmarks from `1k3d68.onnx`.

### DLL Usage Example

Minimal Qt console application using the DLL without graphics. OpenCV opens the
camera only as an example: in a real app, frames can come from any source as
long as they are BGR `cv::Mat` images.

```cpp
#include "FaceRecognitionEngine.h"
#include "FaceTypes.h"
#include "InferenceDevice.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <opencv2/opencv.hpp>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    FaceRecognitionEngine engine(InferenceDevice::CPU);
    engine.setLandmarkMode(static_cast<int>(LandmarkMode::All));

    QObject::connect(&engine,
                     &FaceRecognitionEngine::resultReady,
                     [](const RecognitionResult& result) {
                         qInfo() << "faces:" << result.faces.size()
                                 << "fps:" << result.fps
                                 << "brightness:" << result.brightness;

                         for (const DetectedFace& face : result.faces) {
                             qInfo() << "name:" << face.name
                                     << "confidence:" << face.confidencePercent;
                         }
                     });

    QObject::connect(&engine,
                     &FaceRecognitionEngine::landmarksReady,
                     [](const RecognitionResult& result) {
                         qInfo() << "landmark result for" << result.faces.size() << "faces";
                     });

    const bool ok = engine.initialize(
        "models/det_500m.onnx",
        "models/face_landmark_2d_106.onnx",
        "models/1k3d68.onnx",
        "models/w600k_mbf.onnx",
        "face_embeddings");

    if (!ok) {
        qWarning() << "Cannot initialize FaceRecognitionEngine";
        return 1;
    }

    cv::VideoCapture camera(0);
    if (!camera.isOpened()) {
        qWarning() << "Cannot open camera";
        return 2;
    }

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        if (engine.isBusy()) {
            return;
        }

        cv::Mat frame;
        camera >> frame; // OpenCV returns BGR frames.
        if (!frame.empty()) {
            engine.submitFrame(frame);
        }
    });

    timer.start(30);
    return app.exec();
}
```

Example `.pro` for a host program importing the DLL:

```qmake
QT += core
CONFIG += c++17 console
TEMPLATE = app
TARGET = FaceRecognitionHost

INCLUDEPATH += \
    C:/QT_WORKSPACE/FaceRecognition/src/api \
    C:/QT_WORKSPACE/FaceRecognition/src/inference \
    C:/QT_WORKSPACE/FaceRecognition/src/vision \
    C:/opencv-4.13.0/build/install/include

LIBS += \
    -LC:/path/to/FaceRecognition/build/release -lFaceRecognition \
    -LC:/opencv-4.13.0/build/install/x64/vc17/lib -lopencv_world4130

SOURCES += main.cpp
```

The following must be available next to the host executable:

- `FaceRecognition.dll`;
- `onnxruntime.dll` and, if CUDA is used, the ONNX Runtime providers;
- `opencv_world4130.dll`;
- `models` folder;
- `face_embeddings` folder.

## Build Modes

Run these commands from a Developer Prompt for MSVC x64, or after `vcvars64.bat`.

GUI:

```powershell
qmake FaceRecognition.pro CONFIG+=release CONFIG+=facerecognition_gui
nmake
```

Backend DLL without QML, without `src/ui`, and without QVideoStream:

```powershell
qmake FaceRecognition.pro CONFIG+=release CONFIG+=facerecognition_dll
nmake
```

Alternatively, generate the DLL target directly:

```powershell
qmake FaceRecognitionLib.pro CONFIG+=release
nmake
```

The GUI target produces `FaceRecognition.exe`. The DLL target produces
`FaceRecognition.dll` plus the compiler import library.

## Configured Local Dependencies

- ONNX Runtime: `C:/onnxruntime`
- OpenCV: `C:/opencv-4.13.0/build/install`
- FFmpeg/QVideoStream: `third_party/qvideostream/ffmpeg`

OpenCV is not used by the GUI for video acquisition: in the QML demo, video
capture and rendering come from `VideoStream`/QVideoStream. OpenCV remains in
the pipeline for conversion, preprocessing, detection, alignment, landmarks,
embeddings, and recognition.

`RuntimePaths::resolve(...)` looks for models and data first next to the
executable, then in the working directory, and finally in the source folder when
`APP_SOURCE_DIR` is defined.
