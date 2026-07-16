#include "FaceLandmarker.h"

#include <QDebug>
#include <algorithm>
#include <cmath>

#ifdef USE_CUDA_PROVIDER
#include <onnxruntime_c_api.h>
#endif

FaceLandmarker::FaceLandmarker(InferenceDevice device)
    : m_requestedDevice(device),
      m_actualDevice(InferenceDevice::CPU),
      m_env(ORT_LOGGING_LEVEL_WARNING, "landmark106") {
    m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    m_sessionOptions.SetIntraOpNumThreads(1);
}

bool FaceLandmarker::configureSessionOptions() {
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
            qInfo().nospace() << "FaceLandmarker: CUDA provider enabled";
        } catch (const std::exception& e) {
            qWarning() << "FaceLandmarker: failed to enable CUDA, fallback to CPU:" << e.what();
            m_actualDevice = InferenceDevice::CPU;
        }
#else
        qWarning() << "FaceLandmarker: built without CUDA provider support, using CPU";
        m_actualDevice = InferenceDevice::CPU;
#endif
    } else {
        qInfo().nospace() << "FaceLandmarker: using CPU provider";
    }

    return true;
}

bool FaceLandmarker::load(const QString& modelPath) {
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

        qInfo().nospace() << "FaceLandmarker loaded. Requested "
                          << inferenceDeviceName(m_requestedDevice)
                          << " / Actual " << inferenceDeviceName(m_actualDevice)
                          << " / Input " << m_inputSize.width << "x" << m_inputSize.height;

        return true;
    } catch (const std::exception& e) {
        qWarning() << "FaceLandmarker load failed:" << e.what();
        m_session.reset();
        return false;
    }
}

std::vector<cv::Point2f> FaceLandmarker::detect(const cv::Mat& frame, const cv::Rect& faceRect) const {
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
        if (total < 212) {
            return {};
        }

        cv::Mat inverseTransform;
        cv::invertAffineTransform(transform, inverseTransform);

        std::vector<cv::Point2f> points;
        points.reserve(106);
        for (size_t i = 0; i < 106; ++i) {
            const float cropX = (out[i * 2] + 1.0f) * static_cast<float>(m_inputSize.width) * 0.5f;
            const float cropY = (out[i * 2 + 1] + 1.0f) * static_cast<float>(m_inputSize.height) * 0.5f;
            if (!std::isfinite(cropX) || !std::isfinite(cropY)) {
                return {};
            }

            const double* m = inverseTransform.ptr<double>();
            const float x = static_cast<float>(m[0] * cropX + m[1] * cropY + m[2]);
            const float y = static_cast<float>(m[3] * cropX + m[4] * cropY + m[5]);
            if (!std::isfinite(x) || !std::isfinite(y)) {
                return {};
            }
            points.emplace_back(x, y);
        }
        return points;
    } catch (const std::exception& e) {
        qWarning() << "FaceLandmarker detect failed:" << e.what();
        return {};
    } catch (...) {
        qWarning() << "FaceLandmarker detect failed with unknown error";
        return {};
    }
}

std::array<cv::Point2f, 5> FaceLandmarker::toFivePoints(const std::vector<cv::Point2f>& landmarks106) const {
    if (landmarks106.size() < 106) {
        return {};
    }

    return {
        landmarks106[38],
        landmarks106[88],
        landmarks106[86],
        landmarks106[52],
        landmarks106[61]
    };
}
