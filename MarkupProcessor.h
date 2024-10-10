#pragma once
#ifndef MARKUPPROCESSOR_H
#define MARKUPPROCESSOR_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>  

class MarkupProcessor {
public:
    MarkupProcessor(const std::string& inputDir, const std::string& outputDir);
    void processImages();

private:
    std::string inputDir;
    std::string outputDir;

    void createMask(const cv::Mat& image, cv::Mat& mask);
    std::vector<std::vector<int>> maskToPolygon(const cv::Mat& mask);
    void saveJson(const std::vector<nlohmann::json>& annotations, const std::vector<nlohmann::json>& images);
    void zipOutput();
};

#endif

