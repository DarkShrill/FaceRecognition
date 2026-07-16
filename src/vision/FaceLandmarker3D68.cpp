#include "FaceLandmarker3D68.h"

#include <QDebug>
#include <algorithm>
#include <array>
#include <cmath>

#ifdef USE_CUDA_PROVIDER
#include <onnxruntime_c_api.h>
#endif

FaceLandmarker3D68::FaceLandmarker3D68(InferenceDevice device)
    : m_requestedDevice(device),
      m_actualDevice(InferenceDevice::CPU),
      m_env(ORT_LOGGING_LEVEL_WARNING, "landmark3d68") {
    m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    m_sessionOptions.SetIntraOpNumThreads(1);
}

bool FaceLandmarker3D68::configureSessionOptions() {
    m_actualDevice = InferenceDevice::CPU;

    if (m_requestedDevice == InferenceDevice::CUDA) {
#ifdef USE_CUDA_PROVIDER
        OrtCUDAProviderOptions cudaOptions{};
        cudaOptions.device_id = 0;
        cudaOptions.arena_extend_strategy = 0;
        cudaOptions.gpu_mem_limit = SIZE_MAX;
        cudaOptions.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;
        cudaOptions.do_copy_in_default_stream = 1;

        try {
            m_sessionOptions.AppendExecutionProvider_CUDA(cudaOptions);
            m_actualDevice = InferenceDevice::CUDA;
            qInfo().nospace() << "FaceLandmarker3D68: CUDA provider enabled";
        } catch (const std::exception& e) {
            qWarning() << "FaceLandmarker3D68: failed to enable CUDA, fallback to CPU:" << e.what();
            m_actualDevice = InferenceDevice::CPU;
        }
#else
        qWarning() << "FaceLandmarker3D68: built without CUDA provider support, using CPU";
        m_actualDevice = InferenceDevice::CPU;
#endif
    } else {
        qInfo().nospace() << "FaceLandmarker3D68: using CPU provider";
    }

    return true;
}

bool FaceLandmarker3D68::load(const QString& modelPath) {
    try {
        configureSessionOptions();

#ifdef _WIN32
        const std::wstring w = modelPath.toStdWString();
        m_session = std::make_unique<Ort::Session>(m_env, w.c_str(), m_sessionOptions);
#else
        const std::string s = modelPath.toStdString();
        m_session = std::make_unique<Ort::Session>(m_env, s.c_str(), m_sessionOptions);
#endif

        Ort::AllocatorWithDefaultOptions allocator;
        auto inputName = m_session->GetInputNameAllocated(0, allocator);
        auto outputName = m_session->GetOutputNameAllocated(0, allocator);
        m_inputName = inputName.get();
        m_outputName = outputName.get();

        auto inputInfo = m_session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo();
        const auto inputShape = inputInfo.GetShape();
        if (inputShape.size() == 4 && inputShape[2] > 0 && inputShape[3] > 0) {
            m_inputSize = cv::Size(static_cast<int>(inputShape[3]),
                                   static_cast<int>(inputShape[2]));
        }

        qInfo().nospace() << "FaceLandmarker3D68 loaded. Requested "
                          << inferenceDeviceName(m_requestedDevice)
                          << " / Actual " << inferenceDeviceName(m_actualDevice)
                          << " / Input " << m_inputSize.width << "x" << m_inputSize.height;

        return true;
    } catch (const std::exception& e) {
        qWarning() << "FaceLandmarker3D68 load failed:" << e.what();
        m_session.reset();
        return false;
    }
}

std::vector<cv::Point3f> FaceLandmarker3D68::detect(const cv::Mat& frame, const cv::Rect& faceRect) const {
    try {
        if (!m_session || frame.empty() || faceRect.empty()) {
            return {};
        }

        const cv::Rect imageRect(0, 0, frame.cols, frame.rows);
        const cv::Rect clipped = faceRect & imageRect;
        if (clipped.empty()) {
            return {};
        }

        const float centerX = clipped.x + clipped.width * 0.5f;
        const float centerY = clipped.y + clipped.height * 0.5f;
        const float cropSize = std::max(clipped.width, clipped.height) * 1.5f;
        if (cropSize <= 1.0f) {
            return {};
        }

        const float scale = static_cast<float>(m_inputSize.width) / cropSize;
        cv::Mat transform = (cv::Mat_<double>(2, 3) <<
            scale, 0.0, m_inputSize.width * 0.5 - centerX * scale,
            0.0, scale, m_inputSize.height * 0.5 - centerY * scale);

        cv::Mat crop;
        cv::warpAffine(frame, crop, transform, m_inputSize,
                       cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

        cv::Mat blob = cv::dnn::blobFromImage(crop, 1.0, m_inputSize,
                                              cv::Scalar(0, 0, 0), true, false);
        std::array<int64_t, 4> inputShape = {1, 3, blob.size[2], blob.size[3]};
        std::vector<float> inputValues(blob.ptr<float>(), blob.ptr<float>() + blob.total());

        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            inputValues.data(),
            inputValues.size(),
            inputShape.data(),
            inputShape.size());

        const char* inputNames[] = {m_inputName.c_str()};
        const char* outputNames[] = {m_outputName.c_str()};

        auto outputs = m_session->Run(Ort::RunOptions{nullptr},
                                      inputNames,
                                      &inputTensor,
                                      1,
                                      outputNames,
                                      1);

        const float* out = outputs[0].GetTensorData<float>();
        const auto outShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        size_t total = 1;
        for (const auto dim : outShape) {
            if (dim > 0) {
                total *= static_cast<size_t>(dim);
            }
        }
        constexpr size_t landmarkCount = 68;
        constexpr size_t landmarkDims = 3;
        constexpr size_t landmarkValueCount = landmarkCount * landmarkDims;
        if (total < landmarkValueCount) {
            return {};
        }
        const size_t landmarkOffset = total >= 3000 ? total - landmarkValueCount : 0;

        cv::Mat inverseTransform;
        cv::invertAffineTransform(transform, inverseTransform);

        std::vector<cv::Point3f> points;
        points.reserve(landmarkCount);
        for (size_t i = 0; i < landmarkCount; ++i) {
            const size_t base = landmarkOffset + i * landmarkDims;
            const float cropX = (out[base] + 1.0f) * static_cast<float>(m_inputSize.width) * 0.5f;
            const float cropY = (out[base + 1] + 1.0f) * static_cast<float>(m_inputSize.height) * 0.5f;
            const float z = out[base + 2] * static_cast<float>(m_inputSize.width) * 0.5f;
            if (!std::isfinite(cropX) || !std::isfinite(cropY) || !std::isfinite(z)) {
                return {};
            }

            const double* m = inverseTransform.ptr<double>();
            const float x = static_cast<float>(m[0] * cropX + m[1] * cropY + m[2]);
            const float y = static_cast<float>(m[3] * cropX + m[4] * cropY + m[5]);
            if (!std::isfinite(x) || !std::isfinite(y)) {
                return {};
            }
            points.emplace_back(x, y, z);
        }
        return points;
    } catch (const std::exception& e) {
        qWarning() << "FaceLandmarker3D68 detect failed:" << e.what();
        return {};
    } catch (...) {
        qWarning() << "FaceLandmarker3D68 detect failed with unknown error";
        return {};
    }
}
