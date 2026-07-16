# FaceRecognition Requirements

This project is a C++ Qt/QML application for Windows. Its main dependencies are
not Python packages: they are native MSVC x64 SDKs and libraries.

## Toolchain

- Windows x64.
- Visual Studio 2022 or Build Tools 2022 with the MSVC v143 toolchain.
- Qt 6.9.0 MSVC 2022 64-bit.
- Use the Qt/MSVC kit, not MinGW, because OpenCV is linked from `x64/vc17/lib`.

Typical Qt path on this machine:

```text
C:/Qt/Qt6.9/6.9.0/msvc2022_64
```

## ONNX Runtime GPU

Required version:

```text
ONNX Runtime GPU 1.20.1
```

Automatic check:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\check_requirements.ps1
```

If ONNX Runtime, CUDA, cuDNN, Qt, OpenCV, or FFmpeg are missing, the script
prints what is missing and which version to download. For ONNX Runtime `1.20.1`
GPU, the required combination is:

```text
CUDA Toolkit 12.x
cuDNN 9.x for CUDA 12.x
```

The project looks for ONNX Runtime here:

```text
C:/onnxruntime
```

Official download:

```text
https://github.com/microsoft/onnxruntime/releases/download/v1.20.1/onnxruntime-win-x64-gpu-1.20.1.zip
```

PowerShell installation:

```powershell
$version = "1.20.1"
$url = "https://github.com/microsoft/onnxruntime/releases/download/v$version/onnxruntime-win-x64-gpu-$version.zip"
$zip = "$env:TEMP\onnxruntime-win-x64-gpu-$version.zip"

Invoke-WebRequest -Uri $url -OutFile $zip

if (Test-Path "C:\onnxruntime") {
    Rename-Item "C:\onnxruntime" "onnxruntime_backup_$(Get-Date -Format yyyyMMdd_HHmmss)"
}

Expand-Archive -Path $zip -DestinationPath "C:\"
Rename-Item "C:\onnxruntime-win-x64-gpu-$version" "onnxruntime"
```

Expected files after installation:

```text
C:/onnxruntime/include/onnxruntime_cxx_api.h
C:/onnxruntime/lib/onnxruntime.lib
C:/onnxruntime/lib/onnxruntime.dll
C:/onnxruntime/lib/onnxruntime_providers_shared.lib
C:/onnxruntime/lib/onnxruntime_providers_shared.dll
C:/onnxruntime/lib/onnxruntime_providers_cuda.lib
C:/onnxruntime/lib/onnxruntime_providers_cuda.dll
```

Quick verification:

```powershell
Test-Path C:\onnxruntime\include\onnxruntime_cxx_api.h
Test-Path C:\onnxruntime\lib\onnxruntime.lib
Test-Path C:\onnxruntime\lib\onnxruntime.dll
Test-Path C:\onnxruntime\lib\onnxruntime_providers_cuda.lib
Test-Path C:\onnxruntime\lib\onnxruntime_providers_cuda.dll
```

CUDA/cuDNN note: ONNX Runtime 1.20.x GPU uses CUDA 12.x + cuDNN 9.x as its
default combination. They must be installed and visible in `PATH`, for example:

```text
C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.x/bin
<cuDNN 9.x folder>/bin
```

If `onnxruntime_providers_cuda.lib` is present, `FaceRecognitionApp.pro`
automatically enables:

```qmake
DEFINES += USE_CUDA_PROVIDER
LIBS += -L$$ONNXRUNTIME_ROOT/lib -lonnxruntime_providers_shared -lonnxruntime_providers_cuda
```

If the CUDA provider is not found or cannot be loaded at runtime, the code falls
back to CPU.

## OpenCV

Version/path expected by the project:

```text
C:/opencv-4.13.0/build/install
```

Expected files:

```text
C:/opencv-4.13.0/build/install/include/opencv2/opencv.hpp
C:/opencv-4.13.0/build/install/x64/vc17/lib/opencv_world4130.lib
C:/opencv-4.13.0/build/install/x64/vc17/bin/opencv_world4130.dll
```

Recommended option: keep OpenCV as an external installation at that path, built
or downloaded for MSVC/vc17 x64. Avoid mixing MinGW libraries with this project.

## FFmpeg / QVideoStream

QVideoStream is vendored in the repository:

```text
third_party/qvideostream
```

FFmpeg is expected under:

```text
third_party/qvideostream/ffmpeg
```

Expected files:

```text
third_party/qvideostream/ffmpeg/include/libavformat/avformat.h
third_party/qvideostream/ffmpeg/lib/avformat.lib
third_party/qvideostream/ffmpeg/bin/avformat-58.dll
```

`FaceRecognition.pro` builds `third_party/qvideostream/QVideoStream.pro` first
and then the main application.

## Models And Runtime Data

These are part of the repository and are resolved by `RuntimePaths`:

```text
models/*.onnx
face_embeddings/*
fonts/*.ttf
qml.qrc
```

## Dependency Strategy

The simplest setup for this repository is:

1. keep Qt, ONNX Runtime GPU, and OpenCV as external installations;
2. keep paths aligned with `FaceRecognitionApp.pro`;
3. keep QVideoStream/FFmpeg inside `third_party/qvideostream`;
4. use `windeployqt` after the build to copy Qt/QML DLLs next to the exe;
5. also copy ONNX Runtime, OpenCV, and FFmpeg DLLs next to the exe if they are
   not already in `PATH`.

Possible alternatives:

- Use `vcpkg` or `conan` for OpenCV/FFmpeg, but that requires rewriting part of
  the qmake configuration.
- Vendor ONNX Runtime and OpenCV inside `third_party`, but that makes the
  repository much heavier.
- Make paths configurable through an untracked local `.pri` file, useful when
  different machines use different folders.

For now, the most stable choice is to keep this contract:

```qmake
ONNXRUNTIME_ROOT = C:/onnxruntime
OPENCV_ROOT = C:/opencv-4.13.0/build/install
QVIDEOSTREAM_ROOT = $$PWD/third_party/qvideostream
```
