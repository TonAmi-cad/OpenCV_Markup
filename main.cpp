#include "MarkupProcessor.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace fs = std::filesystem;

std::mutex queueMutex;
std::condition_variable conditionVar;  // Переименовали переменную cv
std::queue<fs::path> taskQueue;
bool finished = false;

void processMarkupTasks(const std::string& outputDir) {
    while (true) {
        fs::path taskPath;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            conditionVar.wait(lock, [] { return !taskQueue.empty() || finished; });

            if (finished && taskQueue.empty()) break;
            taskPath = taskQueue.front();
            taskQueue.pop();
        }

        std::string markupDir = outputDir + "/" + taskPath.filename().string() + "_markup";
        MarkupProcessor processor(taskPath.string(), markupDir);
        processor.processImages();
    }
}

int main() {
    std::string inputDir, outputDir;
    std::cout << "Enter the input directory path: ";
    std::getline(std::cin, inputDir);
    std::cout << "Enter the output directory path: ";
    std::getline(std::cin, outputDir);

    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (entry.is_directory()) {
            std::lock_guard<std::mutex> lock(queueMutex);
            taskQueue.push(entry.path());
        }
    }

    const int numThreads = 3;
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(processMarkupTasks, outputDir);
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        finished = true;
    }
    conditionVar.notify_all();  // Используем новое имя переменной

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "All tasks completed.\n";
    return 0;
}

