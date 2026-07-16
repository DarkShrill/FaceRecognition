#include "FaceDetector.h"
#include <numeric>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

FaceDetector::FaceDetector(InferenceDevice device)
    : m_requestedDevice(device),
    m_actualDevice(InferenceDevice::CPU),
    m_env(ORT_LOGGING_LEVEL_WARNING, "scrfd") {

    m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    m_sessionOptions.SetIntraOpNumThreads(1); // PIU FPS AUMENTARE... 4
}

bool FaceDetector::configureSessionOptions()
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
            qInfo().nospace() << "FaceDetector: CUDA provider enabled";
        } catch (const std::exception& e) {
            qWarning() << "FaceDetector: failed to enable CUDA, fallback to CPU:" << e.what();
            m_actualDevice = InferenceDevice::CPU;
        }
#else
        qWarning() << "FaceDetector: built without CUDA provider support, using CPU";
        m_actualDevice = InferenceDevice::CPU;
#endif
    } else {
        qInfo().nospace() << "FaceDetector: using CPU provider";
    }

    return true;
}

bool FaceDetector::load(const QString& modelPath, const cv::Size& inputSize) {
    try {

        configureSessionOptions();

#ifdef _WIN32
        std::wstring w = modelPath.toStdWString();
        m_session = std::make_unique<Ort::Session>(m_env, w.c_str(), m_sessionOptions);
#else
        std::string s = modelPath.toStdString();
        m_session = std::make_unique<Ort::Session>(m_env, s.c_str(), m_sessionOptions);
#endif
        m_inputSize = inputSize;

        Ort::AllocatorWithDefaultOptions allocator;
        auto inputName = m_session->GetInputNameAllocated(0, allocator);
        m_inputName = QString::fromUtf8(inputName.get());
        m_inputNameStorage = {m_inputName.toStdString()};

        const size_t outputCount = m_session->GetOutputCount();
        m_outputNamesStorage.clear();
        m_outputNames.clear();
        for (size_t i = 0; i < outputCount; ++i) {
            auto out = m_session->GetOutputNameAllocated(i, allocator);
            m_outputNamesStorage.push_back(out.get());
        }
        for (const auto& n : m_outputNamesStorage) {
            m_outputNames.push_back(n.c_str());
        }

        if (outputCount == 6) {
            m_fmc = 3; m_featStrideFpn = {8, 16, 32}; m_numAnchors = 2; m_useKps = false;
        } else if (outputCount == 9) {
            m_fmc = 3; m_featStrideFpn = {8, 16, 32}; m_numAnchors = 2; m_useKps = true;
        } else if (outputCount == 10) {
            m_fmc = 5; m_featStrideFpn = {8, 16, 32, 64, 128}; m_numAnchors = 1; m_useKps = false;
        } else if (outputCount == 15) {
            m_fmc = 5; m_featStrideFpn = {8, 16, 32, 64, 128}; m_numAnchors = 1; m_useKps = true;
        } else {
            throw std::runtime_error("Unsupported SCRFD output count");
        }

        auto outTypeInfo = m_session->GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo();
        m_batched = outTypeInfo.GetShape().size() == 3;

        qInfo().nospace() << "FaceDetector loaded. Requested "
                          << inferenceDeviceName(m_requestedDevice)
                          << " / Actual " << inferenceDeviceName(m_actualDevice)
                          << " / Input " << m_inputSize.width << "x" << m_inputSize.height;


        return true;
    } catch (...) {
        return false;
    }
}

std::vector<cv::Point2f> FaceDetector::getAnchorCenters(int height, int width, int stride) const {
    CenterCacheKey key{height, width, stride};
    auto it = m_centerCache.find(key);
    if (it != m_centerCache.end()) {
        return it->second;
    }

    std::vector<cv::Point2f> centers;
    centers.reserve(height * width * m_numAnchors);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            cv::Point2f pt(static_cast<float>(x * stride), static_cast<float>(y * stride));
            if (m_numAnchors > 1) {
                for (int a = 0; a < m_numAnchors; ++a) {
                    centers.push_back(pt);
                }
            } else {
                centers.push_back(pt);
            }
        }
    }

    if (m_centerCache.size() < 100) {
        m_centerCache.emplace(key, centers);
    }
    return centers;
}

std::vector<cv::Rect2f> FaceDetector::distance2bbox(const std::vector<cv::Point2f>& points, const float* distance, int count) {
    std::vector<cv::Rect2f> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        const auto& p = points[i];
        float x1 = p.x - distance[i * 4 + 0];
        float y1 = p.y - distance[i * 4 + 1];
        float x2 = p.x + distance[i * 4 + 2];
        float y2 = p.y + distance[i * 4 + 3];
        out.emplace_back(x1, y1, x2 - x1, y2 - y1);
    }
    return out;
}

std::vector<cv::Point2f> FaceDetector::distance2kps(const std::vector<cv::Point2f>& points, const float* distance, int count) {
    std::vector<cv::Point2f> out;
    out.reserve(count * 5);
    for (int i = 0; i < count; ++i) {
        const auto& p = points[i];
        for (int k = 0; k < 5; ++k) {
            float x = p.x + distance[i * 10 + k * 2 + 0];
            float y = p.y + distance[i * 10 + k * 2 + 1];
            out.emplace_back(x, y);
        }
    }
    return out;
}

std::vector<int> FaceDetector::nms(const std::vector<cv::Rect2f>& boxes, const std::vector<float>& scores, float threshold) {
    std::vector<int> order(boxes.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return scores[a] > scores[b]; });

    std::vector<int> keep;
    while (!order.empty()) {
        int i = order.front();
        keep.push_back(i);
        std::vector<int> remaining;
        for (size_t oi = 1; oi < order.size(); ++oi) {
            int j = order[oi];
            const auto& A = boxes[i];
            const auto& B = boxes[j];
            float xx1 = std::max(A.x, B.x);
            float yy1 = std::max(A.y, B.y);
            float xx2 = std::min(A.x + A.width, B.x + B.width);
            float yy2 = std::min(A.y + A.height, B.y + B.height);
            float w = std::max(0.0f, xx2 - xx1 + 1.0f);
            float h = std::max(0.0f, yy2 - yy1 + 1.0f);
            float inter = w * h;
            float areaA = (A.width + 1.0f) * (A.height + 1.0f);
            float areaB = (B.width + 1.0f) * (B.height + 1.0f);
            float ovr = inter / (areaA + areaB - inter);
            if (ovr <= threshold) {
                remaining.push_back(j);
            }
        }
        order = std::move(remaining);
    }
    return keep;
}

FaceDetector::ForwardResult FaceDetector::forward(const cv::Mat& detImg) const {
    ForwardResult result;

    cv::Mat blob = cv::dnn::blobFromImage(
        detImg,
        1.0 / m_inputStd,
        cv::Size(detImg.cols, detImg.rows),
        cv::Scalar(m_inputMean, m_inputMean, m_inputMean),
        true,
        false
        );

    std::array<int64_t, 4> inputShape = {1, 3, blob.size[2], blob.size[3]};
    size_t inputTensorSize = static_cast<size_t>(blob.total());
    std::vector<float> inputTensorValues(blob.ptr<float>(), blob.ptr<float>() + inputTensorSize);

    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo,
        inputTensorValues.data(),
        inputTensorValues.size(),
        inputShape.data(),
        inputShape.size()
        );

    const char* inputNames[] = {m_inputNameStorage[0].c_str()};
    auto outputs = m_session->Run(
        Ort::RunOptions{nullptr},
        inputNames,
        &inputTensor,
        1,
        m_outputNames.data(),
        m_outputNames.size()
        );

    const int inputHeight = blob.size[2];
    const int inputWidth  = blob.size[3];

    auto getK = [](const std::vector<int64_t>& shape) -> int {
        if (shape.empty()) {
            throw std::runtime_error("Invalid SCRFD output: empty shape");
        }

        if (shape.size() == 3) {
            // [1, K, C]
            return static_cast<int>(shape[1]);
        }
        if (shape.size() == 2) {
            // [K, C]
            return static_cast<int>(shape[0]);
        }

        throw std::runtime_error("Unsupported SCRFD output rank");
    };

    auto getDim = [](const std::vector<int64_t>& shape) -> int {
        if (shape.empty()) {
            throw std::runtime_error("Invalid SCRFD output: empty shape");
        }
        return static_cast<int>(shape.back());
    };

    auto readValue = [](const float* ptr, int i, int dim, int j) -> float {
        return ptr[i * dim + j];
    };

    for (size_t idx = 0; idx < m_featStrideFpn.size(); ++idx) {
        const int stride = m_featStrideFpn[idx];

        auto scoreInfo = outputs[idx].GetTensorTypeAndShapeInfo();
        auto bboxInfo  = outputs[idx + m_fmc].GetTensorTypeAndShapeInfo();

        std::vector<int64_t> scoreShape = scoreInfo.GetShape();
        std::vector<int64_t> bboxShape  = bboxInfo.GetShape();

        const float* scoresPtr = outputs[idx].GetTensorData<float>();
        const float* bboxPtr   = outputs[idx + m_fmc].GetTensorData<float>();
        const float* kpsPtr    = nullptr;

        if (m_useKps) {
            kpsPtr = outputs[idx + m_fmc * 2].GetTensorData<float>();
        }

        const int height = inputHeight / stride;
        const int width  = inputWidth / stride;

        const int K_scores = getK(scoreShape);
        const int K_bbox   = getK(bboxShape);

        if (K_scores != K_bbox) {
            throw std::runtime_error("SCRFD output mismatch: scores and bbox lengths differ");
        }

        const int K = K_scores;

        const int expectedK = height * width * m_numAnchors;
        if (expectedK != K) {
            std::cerr
                << "SCRFD warning: expected K=" << expectedK
                << " but model returned K=" << K
                << " at stride=" << stride
                << " (input=" << inputWidth << "x" << inputHeight << ")"
                << std::endl;
        }

        const int scoreDim = getDim(scoreShape);
        const int bboxDim  = getDim(bboxShape);

        if (bboxDim < 4) {
            throw std::runtime_error("SCRFD bbox output dimension < 4");
        }

        std::vector<float> scoreVals(K);
        for (int i = 0; i < K; ++i) {
            scoreVals[i] = (scoreDim == 1)
            ? readValue(scoresPtr, i, scoreDim, 0)
            : readValue(scoresPtr, i, scoreDim, scoreDim - 1);
        }

        std::vector<float> bboxVals(K * 4);
        for (int i = 0; i < K; ++i) {
            for (int j = 0; j < 4; ++j) {
                bboxVals[i * 4 + j] = readValue(bboxPtr, i, bboxDim, j) * static_cast<float>(stride);
            }
        }

        std::vector<float> kpsVals;
        if (m_useKps) {
            auto kpsShape = outputs[idx + m_fmc * 2].GetTensorTypeAndShapeInfo().GetShape();
            const int K_kps = getK(kpsShape);
            const int kpsDim = getDim(kpsShape);

            if (K_kps != K) {
                throw std::runtime_error("SCRFD output mismatch: kps length differs from scores");
            }
            if (kpsDim < 10) {
                throw std::runtime_error("SCRFD kps output dimension < 10");
            }

            kpsVals.resize(K * 10);
            for (int i = 0; i < K; ++i) {
                for (int j = 0; j < 10; ++j) {
                    kpsVals[i * 10 + j] = readValue(kpsPtr, i, kpsDim, j) * static_cast<float>(stride);
                }
            }
        }

        auto anchorCenters = getAnchorCenters(height, width, stride);

        // Se il modello restituisce un K diverso, evitiamo out-of-bounds
        if (static_cast<int>(anchorCenters.size()) < K) {
            throw std::runtime_error("SCRFD anchor center count is smaller than K");
        }

        auto boxes = distance2bbox(anchorCenters, bboxVals.data(), K);

        std::vector<cv::Point2f> kpsFlat;
        if (m_useKps) {
            kpsFlat = distance2kps(anchorCenters, kpsVals.data(), K);
        }

        for (int i = 0; i < K; ++i) {
            if (scoreVals[i] < m_detThresh) {
                continue;
            }

            result.scores.push_back(scoreVals[i]);
            result.bboxes.push_back(boxes[i]);

            if (m_useKps) {
                std::array<cv::Point2f, 5> faceKps{};
                for (int k = 0; k < 5; ++k) {
                    faceKps[k] = kpsFlat[i * 5 + k];
                }
                result.kpss.push_back(faceKps);
            }
        }
    }

    return result;
}

std::vector<DetectedFace> FaceDetector::detect(const cv::Mat& image, int maxNum, const QString& metric) const {
    if (!m_session || image.empty()) {
        return {};
    }

    const float imRatio = static_cast<float>(image.rows) / static_cast<float>(image.cols);
    const float modelRatio = static_cast<float>(m_inputSize.height) / static_cast<float>(m_inputSize.width);

    int newWidth = 0;
    int newHeight = 0;
    if (imRatio > modelRatio) {
        newHeight = m_inputSize.height;
        newWidth = static_cast<int>(newHeight / imRatio);
    } else {
        newWidth = m_inputSize.width;
        newHeight = static_cast<int>(newWidth * imRatio);
    }

    float detScale = static_cast<float>(newHeight) / static_cast<float>(image.rows);
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(newWidth, newHeight));
    cv::Mat detImg(m_inputSize.height, m_inputSize.width, CV_8UC3, cv::Scalar(0, 0, 0));
    resized.copyTo(detImg(cv::Rect(0, 0, resized.cols, resized.rows)));

    auto forwardResult = forward(detImg);
    if (forwardResult.scores.empty()) {
        return {};
    }

    std::vector<int> order(forwardResult.scores.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return forwardResult.scores[a] > forwardResult.scores[b];
    });

    std::vector<cv::Rect2f> orderedBoxes;
    std::vector<float> orderedScores;
    std::vector<std::array<cv::Point2f, 5>> orderedKps;
    orderedBoxes.reserve(order.size());
    orderedScores.reserve(order.size());
    orderedKps.reserve(order.size());

    for (int idx : order) {
        cv::Rect2f box = forwardResult.bboxes[idx];
        box.x /= detScale;
        box.y /= detScale;
        box.width /= detScale;
        box.height /= detScale;
        orderedBoxes.push_back(box);
        orderedScores.push_back(forwardResult.scores[idx]);
        if (m_useKps) {
            auto kps = forwardResult.kpss[idx];
            for (auto& p : kps) {
                p.x /= detScale;
                p.y /= detScale;
            }
            orderedKps.push_back(kps);
        }
    }

    auto keep = nms(orderedBoxes, orderedScores, m_nmsThresh);
    std::vector<DetectedFace> faces;
    faces.reserve(keep.size());
    for (int idx : keep) {
        DetectedFace f;
        f.bbox = orderedBoxes[idx];
        if (m_useKps) {
            f.kps = orderedKps[idx];
        }
        faces.push_back(f);
    }

    if (maxNum > 0 && static_cast<int>(faces.size()) > maxNum) {
        std::vector<std::pair<float, int>> ranked;
        ranked.reserve(faces.size());
        cv::Point2f center(image.cols * 0.5f, image.rows * 0.5f);
        for (int i = 0; i < static_cast<int>(faces.size()); ++i) {
            const auto& b = faces[i].bbox;
            float area = b.width * b.height;
            cv::Point2f c(b.x + b.width * 0.5f, b.y + b.height * 0.5f);
            float d2 = (c.x - center.x) * (c.x - center.x) + (c.y - center.y) * (c.y - center.y);
            float value = (metric == "max") ? area : (area - d2 * 2.0f);
            ranked.push_back({value, i});
        }
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        std::vector<DetectedFace> limited;
        for (int i = 0; i < maxNum; ++i) {
            limited.push_back(faces[ranked[i].second]);
        }
        return limited;
    }

    return faces;
}
