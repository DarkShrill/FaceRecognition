#include "FaceAligner.h"

std::array<cv::Point2f, 5> FaceAligner::targetPoints(int imageSize) const {
    const std::array<cv::Point2f, 5> base = {
        cv::Point2f(38.2946f, 51.6963f),
        cv::Point2f(73.5318f, 51.5014f),
        cv::Point2f(56.0252f, 71.7366f),
        cv::Point2f(41.5493f, 92.3655f),
        cv::Point2f(70.7299f, 92.2041f)
    };

    float ratio = 1.0f;
    float diffX = 0.0f;
    if (imageSize % 112 == 0) {
        ratio = static_cast<float>(imageSize) / 112.0f;
    } else {
        ratio = static_cast<float>(imageSize) / 128.0f;
        diffX = 8.0f * ratio;
    }

    std::array<cv::Point2f, 5> dst = base;
    for (auto& p : dst) {
        p.x = p.x * ratio + diffX;
        p.y = p.y * ratio;
    }
    return dst;
}

cv::Mat FaceAligner::normCrop(const cv::Mat& image,
                              const std::array<cv::Point2f, 5>& srcPts,
                              int imageSize) const {
    auto dstPts = targetPoints(imageSize);

    std::vector<cv::Point2f> src(srcPts.begin(), srcPts.end());
    std::vector<cv::Point2f> dst(dstPts.begin(), dstPts.end());

    cv::Mat inliers;
    cv::Mat M = cv::estimateAffinePartial2D(src, dst, inliers, cv::LMEDS);
    if (M.empty()) {
        return cv::Mat();
    }

    cv::Mat aligned;
    cv::warpAffine(image, aligned, M, cv::Size(imageSize, imageSize),
                   cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    return aligned;
}
