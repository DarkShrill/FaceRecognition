param(
    [string]$OnnxRuntimeRoot = "C:\onnxruntime",
    [string]$OpenCvRoot = "C:\opencv-4.13.0\build\install",
    [string]$QtRoot = "C:\Qt\Qt6.9\6.9.3\msvc2022_64",
    [string]$ModelsRoot = ""
)

$ErrorActionPreference = "Stop"

$onnxRuntimeVersion = "1.20.1"
$onnxRuntimeGpuZip = "onnxruntime-win-x64-gpu-$onnxRuntimeVersion.zip"
$onnxRuntimeGpuUrl = "https://github.com/microsoft/onnxruntime/releases/download/v$onnxRuntimeVersion/$onnxRuntimeGpuZip"
$cudaRequiredMajor = 12
$cudnnRequiredMajor = 9
$missingCount = 0
$repoRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($ModelsRoot)) {
    $ModelsRoot = Join-Path $repoRoot "models"
}

function Write-Ok {
    param([string]$Message)
    Write-Host "[OK]      $Message" -ForegroundColor Green
}

function Write-Warn {
    param([string]$Message)
    Write-Host "[WARN]    $Message" -ForegroundColor Yellow
}

function Write-Missing {
    param([string]$Message)
    $script:missingCount++
    Write-Host "[MISSING] $Message" -ForegroundColor Red
}

function Test-RequiredFile {
    param(
        [string]$Path,
        [string]$MissingMessage
    )

    if (Test-Path -LiteralPath $Path) {
        Write-Ok $Path
        return $true
    }

    Write-Missing $MissingMessage
    Write-Host "          Expected: $Path"
    return $false
}

function Get-PathEntries {
    if ([string]::IsNullOrWhiteSpace($env:PATH)) {
        return @()
    }

    return $env:PATH.Split(";") | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
}

function Find-Executable {
    param([string]$Name)

    foreach ($entry in Get-PathEntries) {
        $candidate = Join-Path $entry $Name
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return $null
}

function Find-DllInPath {
    param([string]$Name)

    foreach ($entry in Get-PathEntries) {
        $candidate = Join-Path $entry $Name
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return $null
}


function Copy-ModelFromExtractedArchive {
    param(
        [string]$ExtractRoot,
        [string]$SourceName,
        [string]$DestinationName = $SourceName
    )

    $source = Get-ChildItem -LiteralPath $ExtractRoot -Recurse -File -Filter $SourceName -ErrorAction SilentlyContinue |
        Select-Object -First 1

    if (-not $source) {
        throw "Il modello '$SourceName' non e stato trovato nell'archivio estratto."
    }

    $destination = Join-Path $ModelsRoot $DestinationName
    Copy-Item -LiteralPath $source.FullName -Destination $destination -Force
    Write-Ok "Modello installato: $destination"
}

function Install-InsightFacePackage {
    param(
        [string]$PackageName,
        [string]$Url,
        [System.Collections.IDictionary]$ModelsToCopy
    )

    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("insightface_" + [guid]::NewGuid().ToString("N"))
    $zipPath = Join-Path $tempRoot "$PackageName.zip"
    $extractRoot = Join-Path $tempRoot "extracted"

    try {
        New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
        New-Item -ItemType Directory -Path $extractRoot -Force | Out-Null
        New-Item -ItemType Directory -Path $ModelsRoot -Force | Out-Null

        Write-Host "          Download di $PackageName da GitHub..."
        Invoke-WebRequest -Uri $Url -OutFile $zipPath -UseBasicParsing

        Write-Host "          Estrazione di $PackageName..."
        Expand-Archive -LiteralPath $zipPath -DestinationPath $extractRoot -Force

        foreach ($sourceName in $ModelsToCopy.Keys) {
            Copy-ModelFromExtractedArchive `
                -ExtractRoot $extractRoot `
                -SourceName $sourceName `
                -DestinationName $ModelsToCopy[$sourceName]
        }
    } finally {
        if (Test-Path -LiteralPath $tempRoot) {
            Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

function Test-InsightFaceModels {
    $buffaloSUrl = "https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_s.zip"
    $buffaloLUrl = "https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_l.zip"

    # genderage.onnx e intenzionalmente escluso dai modelli obbligatori.
    $buffaloSModels = [ordered]@{
        "1k3d68.onnx"    = "1k3d68.onnx"
        "2d106det.onnx"  = "2d106det.onnx"
        "det_500m.onnx"  = "det_500m.onnx"
        "w600k_mbf.onnx" = "w600k_mbf.onnx"
    }

    $requiredModels = @(
        "1k3d68.onnx",
        "2d106det.onnx",
        "det_500m.onnx",
        "face_landmark_2d_106.onnx",
        "w600k_mbf.onnx",
        "w600k_r50.onnx"
    )

    New-Item -ItemType Directory -Path $ModelsRoot -Force | Out-Null

    $missingBuffaloS = $false
    foreach ($name in @("1k3d68.onnx", "2d106det.onnx", "det_500m.onnx", "w600k_mbf.onnx")) {
        if (-not (Test-Path -LiteralPath (Join-Path $ModelsRoot $name))) {
            $missingBuffaloS = $true
            break
        }
    }

    # face_landmark_2d_106.onnx e lo stesso modello 2D-106 distribuito come 2d106det.onnx.
    $landmarkAlias = Join-Path $ModelsRoot "face_landmark_2d_106.onnx"
    if (-not (Test-Path -LiteralPath $landmarkAlias)) {
        $source2d106 = Join-Path $ModelsRoot "2d106det.onnx"
        if (Test-Path -LiteralPath $source2d106) {
            Copy-Item -LiteralPath $source2d106 -Destination $landmarkAlias -Force
            Write-Ok "Alias modello creato: $landmarkAlias"
        } else {
            $missingBuffaloS = $true
        }
    }

    if ($missingBuffaloS) {
        try {
            Install-InsightFacePackage -PackageName "buffalo_s" -Url $buffaloSUrl -ModelsToCopy $buffaloSModels

            $source2d106 = Join-Path $ModelsRoot "2d106det.onnx"
            if (Test-Path -LiteralPath $source2d106) {
                Copy-Item -LiteralPath $source2d106 -Destination $landmarkAlias -Force
                Write-Ok "Alias modello creato: $landmarkAlias"
            }
        } catch {
            Write-Warn "Download/installazione di buffalo_s non riuscita: $($_.Exception.Message)"
        }
    }

    $r50Path = Join-Path $ModelsRoot "w600k_r50.onnx"
    if (-not (Test-Path -LiteralPath $r50Path)) {
        try {
            Install-InsightFacePackage `
                -PackageName "buffalo_l" `
                -Url $buffaloLUrl `
                -ModelsToCopy ([ordered]@{ "w600k_r50.onnx" = "w600k_r50.onnx" })
        } catch {
            Write-Warn "Download/installazione di buffalo_l non riuscita: $($_.Exception.Message)"
        }
    }

    foreach ($name in $requiredModels) {
        $path = Join-Path $ModelsRoot $name
        if (Test-Path -LiteralPath $path) {
            Write-Ok $path
        } else {
            Write-Missing "Modello InsightFace mancante: $name"
            Write-Host "          Cartella prevista: $ModelsRoot"
        }
    }
}

function Get-CudaInstallations {
    $roots = @()

    if (-not [string]::IsNullOrWhiteSpace($env:CUDA_PATH)) {
        $roots += $env:CUDA_PATH
    }

    $defaultRoot = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA"
    if (Test-Path -LiteralPath $defaultRoot) {
        Get-ChildItem -LiteralPath $defaultRoot -Directory -ErrorAction SilentlyContinue |
            ForEach-Object { $roots += $_.FullName }
    }

    return $roots | Sort-Object -Unique
}

function Get-CudaVersionFromPath {
    param([string]$Path)

    if ($Path -match "v(?<major>\d+)\.(?<minor>\d+)") {
        return [pscustomobject]@{
            Major = [int]$Matches.major
            Minor = [int]$Matches.minor
            Text = "$($Matches.major).$($Matches.minor)"
        }
    }

    return $null
}

Write-Host ""
Write-Host "FaceRecognition native requirements check"
Write-Host "========================================="
Write-Host ""

Write-Host "ONNX Runtime GPU"
Write-Host "----------------"
$onnxFiles = @(
    "include\onnxruntime_cxx_api.h",
    "lib\onnxruntime.lib",
    "lib\onnxruntime.dll",
    "lib\onnxruntime_providers_shared.lib",
    "lib\onnxruntime_providers_shared.dll",
    "lib\onnxruntime_providers_cuda.lib",
    "lib\onnxruntime_providers_cuda.dll"
)

foreach ($relative in $onnxFiles) {
    $fullPath = Join-Path $OnnxRuntimeRoot $relative
    [void](Test-RequiredFile -Path $fullPath -MissingMessage "ONNX Runtime GPU $onnxRuntimeVersion non trovato o incompleto.")
}

if ($missingCount -gt 0) {
    Write-Host ""
    Write-Host "          Scarica ONNX Runtime GPU ${onnxRuntimeVersion}:"
    Write-Host "          $onnxRuntimeGpuUrl"
    Write-Host "          Estrai lo zip e rinomina la cartella in:"
    Write-Host "          $OnnxRuntimeRoot"
}

Write-Host ""
Write-Host "NVIDIA driver"
Write-Host "-------------"
$nvidiaSmi = Find-Executable "nvidia-smi.exe"
if (-not $nvidiaSmi) {
    $defaultNvidiaSmi = "C:\Program Files\NVIDIA Corporation\NVSMI\nvidia-smi.exe"
    if (Test-Path -LiteralPath $defaultNvidiaSmi) {
        $nvidiaSmi = $defaultNvidiaSmi
    }
}

if ($nvidiaSmi) {
    Write-Ok "nvidia-smi trovato: $nvidiaSmi"
    try {
        $driverInfo = & $nvidiaSmi --query-gpu=name,driver_version --format=csv,noheader 2>$null | Select-Object -First 1
        if (-not [string]::IsNullOrWhiteSpace($driverInfo)) {
            Write-Host "          GPU: $driverInfo"
        }
    } catch {
        Write-Warn "nvidia-smi esiste ma non ha restituito informazioni GPU."
    }
} else {
    Write-Missing "Driver NVIDIA/GPU non rilevati."
    Write-Host "          Installa o aggiorna il driver NVIDIA prima di usare il provider CUDA."
    Write-Host "          Download: https://www.nvidia.com/Download/index.aspx"
}

Write-Host ""
Write-Host "CUDA Toolkit"
Write-Host "------------"
$cudaRoots = Get-CudaInstallations
$cuda12Roots = @()

foreach ($root in $cudaRoots) {
    $version = Get-CudaVersionFromPath $root
    if ($version -and $version.Major -eq $cudaRequiredMajor) {
        $cuda12Roots += $root
    }
}

if ($cuda12Roots.Count -gt 0) {
    foreach ($root in $cuda12Roots) {
        Write-Ok "CUDA $cudaRequiredMajor.x trovato: $root"
    }
} else {
    Write-Missing "CUDA Toolkit $cudaRequiredMajor.x non trovato."
    Write-Host "          ONNX Runtime $onnxRuntimeVersion GPU richiede CUDA $cudaRequiredMajor.x."
    Write-Host "          Scarica CUDA Toolkit $cudaRequiredMajor.x da:"
    Write-Host "          https://developer.nvidia.com/cuda-downloads"
    Write-Host "          oppure dall'archivio versioni:"
    Write-Host "          https://developer.nvidia.com/cuda-toolkit-archive"
}

$cudart = Find-DllInPath "cudart64_12.dll"
if ($cudart) {
    Write-Ok "cudart64_12.dll visibile nel PATH: $cudart"
} else {
    Write-Missing "cudart64_12.dll non visibile nel PATH."
    Write-Host "          Aggiungi al PATH la cartella bin della tua installazione CUDA $cudaRequiredMajor.x,"
    Write-Host "          per esempio C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8\bin"
}

Write-Host ""
Write-Host "cuDNN"
Write-Host "-----"
$cudnn = Find-DllInPath "cudnn64_$cudnnRequiredMajor.dll"
if ($cudnn) {
    Write-Ok "cuDNN $cudnnRequiredMajor.x visibile nel PATH: $cudnn"
} else {
    Write-Missing "cuDNN $cudnnRequiredMajor.x non visibile nel PATH."
    Write-Host "          ONNX Runtime $onnxRuntimeVersion GPU richiede cuDNN $cudnnRequiredMajor.x con CUDA $cudaRequiredMajor.x."
    Write-Host "          Scarica cuDNN $cudnnRequiredMajor.x per CUDA $cudaRequiredMajor.x da:"
    Write-Host "          https://developer.nvidia.com/cudnn"
    Write-Host "          Poi aggiungi al PATH la cartella che contiene cudnn64_$cudnnRequiredMajor.dll."
}

Write-Host ""
Write-Host "Qt"
Write-Host "--"
[void](Test-RequiredFile -Path (Join-Path $QtRoot "bin\qmake.exe") -MissingMessage "Qt 6.9.0 MSVC2022 64-bit non trovato.")
if (-not (Test-Path -LiteralPath (Join-Path $QtRoot "bin\qmake.exe"))) {
    Write-Host "          Installa Qt 6.9.0 con componente MSVC 2022 64-bit dal Qt Maintenance Tool."
}

Write-Host ""
Write-Host "OpenCV"
Write-Host "------"
[void](Test-RequiredFile -Path (Join-Path $OpenCvRoot "include\opencv2\opencv.hpp") -MissingMessage "OpenCV headers non trovati.")
[void](Test-RequiredFile -Path (Join-Path $OpenCvRoot "x64\vc17\lib\opencv_world4130.lib") -MissingMessage "OpenCV 4.13.0 MSVC/vc17 lib non trovata.")
[void](Test-RequiredFile -Path (Join-Path $OpenCvRoot "x64\vc17\bin\opencv_world4130.dll") -MissingMessage "OpenCV 4.13.0 runtime DLL non trovata.")
if (-not (Test-Path -LiteralPath (Join-Path $OpenCvRoot "x64\vc17\lib\opencv_world4130.lib"))) {
    Write-Host "          Installa o compila OpenCV 4.13.0 per MSVC/vc17 x64 in:"
    Write-Host "          $OpenCvRoot"
}


Write-Host ""
Write-Host "InsightFace models"
Write-Host "------------------"
Write-Host "          Cartella modelli: $ModelsRoot"
Test-InsightFaceModels

Write-Host ""
Write-Host "QVideoStream / FFmpeg"
Write-Host "---------------------"
$ffmpegRoot = Join-Path $repoRoot "third_party\qvideostream\ffmpeg"
[void](Test-RequiredFile -Path (Join-Path $repoRoot "third_party\qvideostream\QVideoStream.pro") -MissingMessage "QVideoStream non trovato nel repository.")
[void](Test-RequiredFile -Path (Join-Path $ffmpegRoot "include\libavformat\avformat.h") -MissingMessage "FFmpeg headers vendorizzati non trovati.")
[void](Test-RequiredFile -Path (Join-Path $ffmpegRoot "lib\avformat.lib") -MissingMessage "FFmpeg import lib vendorizzata non trovata.")
[void](Test-RequiredFile -Path (Join-Path $ffmpegRoot "bin\avformat-58.dll") -MissingMessage "FFmpeg runtime DLL vendorizzata non trovata.")

Write-Host ""
if ($missingCount -eq 0) {
    Write-Ok "Tutte le dipendenze native principali risultano presenti."
    exit 0
}

Write-Host "[MISSING] $missingCount controllo/i non superato/i. Installa le dipendenze indicate sopra e rilancia questo script." -ForegroundColor Red
exit 1
