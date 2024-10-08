#pragma execution_character_set("utf-8")

#include <opencv2/opencv.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>
#include <zip.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

// Configuration parameters
const cv::Scalar LOWER_WHITE(0, 0, 200);  // Lower boundary for white in HSV
const cv::Scalar UPPER_WHITE(180, 20, 255); // Upper boundary for white in HSV
const cv::Size KERNEL_SIZE(5, 5);  // Kernel size for morphological operations

void createMask(const cv::Mat& image, cv::Mat& mask) {
    cv::Mat hsv;
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);

    cv::inRange(hsv, LOWER_WHITE, UPPER_WHITE, mask);
    cv::bitwise_not(mask, mask);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, KERNEL_SIZE);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
}

std::vector<std::vector<int>> maskToPolygon(const cv::Mat& mask) {
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

void saveJson(const std::vector<json>& annotations, const std::vector<json>& images, const std::string& path) {
    json output;
    output["annotations"] = annotations;
    output["categories"] = { { {"id", 1}, {"name", "bird"} } };
    output["images"] = images;

    std::ofstream file(path);
    file << output.dump(4);
}

void addFileToZip(zip_t* zip, const std::string& path, const std::string& zipPath) {
    zip_source_t* source = zip_source_file(zip, path.c_str(), 0, 0);
    if (source == nullptr || zip_file_add(zip, zipPath.c_str(), source, ZIP_FL_OVERWRITE) < 0) {
        zip_source_free(source);
        zip_close(zip);
        throw std::runtime_error("Error adding file to zip: " + path);
    }
}

void processImages(const std::string& inputDir, const std::string& outputDir) {
    fs::create_directories(outputDir + "/annotations");
    fs::create_directories(outputDir + "/images");

    std::vector<json> annotations;
    std::vector<json> images;  // List to store image metadata
    int imageId = 1;  // Start image IDs from 1 (or any desired integer)
    int annotationId = 1;  // Annotation IDs must also be unique integers

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

            // Fill annotation with integer ID for image_id
            json annotation;
            annotation["id"] = annotationId;  // Unique integer ID for each annotation
            annotation["image_id"] = imageId;  // Must be an integer
            annotation["category_id"] = 1;
            annotation["segmentation"] = polygons;
            annotation["iscrowd"] = 0;  // Set to 0, as it's not a crowd
            annotations.push_back(annotation);

            // Fill image metadata
            json imageInfo;
            imageInfo["id"] = imageId;  // Must be an integer
            imageInfo["file_name"] = imageName;
            imageInfo["width"] = image.cols;
            imageInfo["height"] = image.rows;
            images.push_back(imageInfo);  // Add to images list

            // Increment IDs
            imageId++;
            annotationId++;
        }
    }

    // Save annotations and images data to JSON
    saveJson(annotations, images, outputDir + "/annotations/instances_default.json");

    std::string zipFilePath = outputDir + "/annotations.zip";
    int error;
    zip_t* zip = zip_open(zipFilePath.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (!zip) {
        throw std::runtime_error("Error creating zip: " + std::string(zip_strerror(zip)));
    }

    addFileToZip(zip, outputDir + "/annotations/instances_default.json", "annotations/instances_default.json");

    for (const auto& entry : fs::directory_iterator(outputDir + "/images")) {
        if (entry.is_regular_file()) {
            addFileToZip(zip, entry.path().string(), "images/" + entry.path().filename().string());
        }
    }

    zip_close(zip);
}

int main() {
    std::string inputDir;
    std::string outputDir;

    // Get input directory from console
    std::cout << "Enter the input directory path: ";
    std::getline(std::cin, inputDir);

    // Get output directory from console
    std::cout << "Enter the output directory path: ";
    std::getline(std::cin, outputDir);

    try {
        processImages(inputDir, outputDir);
        std::cout << "Process completed successfully, zip saved at " << outputDir << "/annotations.zip" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
