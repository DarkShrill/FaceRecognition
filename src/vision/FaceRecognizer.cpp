#include "FaceRecognizer.h"

#ifdef USE_CUDA_PROVIDER
#include <onnxruntime_c_api.h>
#endif

FaceRecognizer::FaceRecognizer(InferenceDevice device)
    : m_requestedDevice(device),
    m_actualDevice(InferenceDevice::CPU),
    m_env(ORT_LOGGING_LEVEL_WARNING, "arcface")
{
    m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    m_sessionOptions.SetIntraOpNumThreads(1); // PIU FPS AUMENTARE... 4
}
bool FaceRecognizer::configureSessionOptions()
{
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
            qInfo().nospace() << "FaceRecognizer: CUDA provider enabled";
        } catch (const std::exception& e) {
            qWarning() << "FaceRecognizer: failed to enable CUDA, fallback to CPU:" << e.what();
            m_actualDevice = InferenceDevice::CPU;
        }
#else
        qWarning() << "FaceRecognizer: built without CUDA provider support, using CPU";
        m_actualDevice = InferenceDevice::CPU;
#endif
    } else {
        qInfo().nospace() << "FaceRecognizer: using CPU provider";
    }

    return true;
}

bool FaceRecognizer::load(const QString& modelPath) {
    try {

        configureSessionOptions();

#ifdef _WIN32
        std::wstring w = modelPath.toStdWString();
        m_session = std::make_unique<Ort::Session>(m_env, w.c_str(), m_sessionOptions);
#else
        std::string s = modelPath.toStdString();
        m_session = std::make_unique<Ort::Session>(m_env, s.c_str(), m_sessionOptions);
#endif
        Ort::AllocatorWithDefaultOptions allocator;
        auto in = m_session->GetInputNameAllocated(0, allocator);
        auto out = m_session->GetOutputNameAllocated(0, allocator);
        m_inputName = in.get();
        m_outputName = out.get();

        auto inputInfo = m_session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo();
        auto shape = inputInfo.GetShape();
        if (shape.size() >= 4 && shape[2] > 0 && shape[3] > 0) {
            m_inputSize = cv::Size(static_cast<int>(shape[3]), static_cast<int>(shape[2]));
        }


        qInfo().nospace() << "FaceRecognizer loaded. Requested "
                          << inferenceDeviceName(m_requestedDevice)
                          << " / Actual " << inferenceDeviceName(m_actualDevice)
                          << " / Input " << m_inputSize.width << "x" << m_inputSize.height;

        return true;
    } catch (...) {
        return false;
    }
}

std::vector<float> FaceRecognizer::extract(const cv::Mat& alignedFace) const {
    if (!m_session || alignedFace.empty()) {
        return {};
    }

    cv::Mat resized;
    if (alignedFace.size() != m_inputSize) {
        cv::resize(alignedFace, resized, m_inputSize);
    } else {
        resized = alignedFace;
    }

    cv::Mat blob = cv::dnn::blobFromImage(resized, 1.0 / m_inputStd, m_inputSize,
                                          cv::Scalar(m_inputMean, m_inputMean, m_inputMean), true, false);

    std::array<int64_t, 4> inputShape = {1, 3, blob.size[2], blob.size[3]};
    std::vector<float> inputTensorValues(blob.ptr<float>(), blob.ptr<float>() + blob.total());

    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(memoryInfo,
                                                             inputTensorValues.data(), inputTensorValues.size(),
                                                             inputShape.data(), inputShape.size());

    const char* inputNames[] = {m_inputName.c_str()};
    const char* outputNames[] = {m_outputName.c_str()};
    auto outputs = m_session->Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);

    const float* out = outputs[0].GetTensorData<float>();
    auto outShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
    size_t total = 1;
    for (auto dim : outShape) {
        if (dim > 0) total *= static_cast<size_t>(dim);
    }
    return std::vector<float>(out, out + total);
}
