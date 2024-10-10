#include "MarkupProcessor.h"
#include "ZipHelper.h"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <fstream>
#include <zip.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

MarkupProcessor::MarkupProcessor(const std::string& inputDir, const std::string& outputDir)
    : inputDir(inputDir), outputDir(outputDir) {}

void MarkupProcessor::createMask(const cv::Mat& image, cv::Mat& mask) {
    const cv::Scalar LOWER_WHITE(0, 0, 200);
    const cv::Scalar UPPER_WHITE(180, 20, 255);
    const cv::Size KERNEL_SIZE(5, 5);

    cv::Mat hsv;
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
    cv::inRange(hsv, LOWER_WHITE, UPPER_WHITE, mask);
    cv::bitwise_not(mask, mask);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, KERNEL_SIZE);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
}

std::vector<std::vector<int>> MarkupProcessor::maskToPolygon(const cv::Mat& mask) {
    std::vector<std::vector<int>> polygons;
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (const auto& contour : contours) {
        std::vector<int> polygon;
        for (const auto& point : contour) {
            polygon.push_back(point.x);
            polygon.push_back(point.y);
        }
        polygons.push_back(polygon);
    }
    return polygons;
}

void MarkupProcessor::saveJson(const std::vector<json>& annotations, const std::vector<json>& images) {
    json output;
    output["annotations"] = annotations;
    output["categories"] = { { {"id", 1}, {"name", "drone"} } };
    output["images"] = images;

    std::ofstream file(outputDir + "/annotations/instances_default.json");
    file << output.dump(4);
}

void MarkupProcessor::zipOutput() {
    zip_t* zip = zip_open((outputDir + "/annotations.zip").c_str(), ZIP_CREATE | ZIP_TRUNCATE, nullptr);
    if (!zip) throw std::runtime_error("Error creating zip");

    addFileToZip(zip, outputDir + "/annotations/instances_default.json", "annotations/instances_default.json");

    for (const auto& entry : fs::directory_iterator(outputDir + "/images")) {
        if (entry.is_regular_file()) {
            addFileToZip(zip, entry.path().string(), "images/" + entry.path().filename().string());
        }
    }
    zip_close(zip);
}

void MarkupProcessor::processImages() {
    fs::create_directories(outputDir + "/annotations");
    fs::create_directories(outputDir + "/images");

    std::vector<json> annotations;
    std::vector<json> images;
    int imageId = 1;
    int annotationId = 1;

    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (entry.is_regular_file()) {
            std::string imagePath = entry.path().string();
            std::string imageName = entry.path().filename().string();

            cv::Mat image = cv::imread(imagePath);
            if (image.empty()) continue;

            cv::Mat mask;
            createMask(image, mask);
            cv::imwrite(outputDir + "/images/" + imageName, image);

            std::vector<std::vector<int>> polygons = maskToPolygon(mask);

            json annotation;
            annotation["id"] = annotationId++;
            annotation["image_id"] = imageId;
            annotation["category_id"] = 1;
            annotation["segmentation"] = polygons;
            annotation["iscrowd"] = 0;
            annotations.push_back(annotation);

            json imageInfo;
            imageInfo["id"] = imageId++;
            imageInfo["file_name"] = imageName;
            imageInfo["width"] = image.cols;
            imageInfo["height"] = image.rows;
            images.push_back(imageInfo);
        }
    }
    saveJson(annotations, images);
    zipOutput();
}
