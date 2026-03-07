#pragma once

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <NvInfer.h>
#include <cuda_runtime_api.h>
#include "ai_structures.hpp"

// Logger required by TensorRT
class Logger : public nvinfer1::ILogger {
    void log(Severity severity, const char* msg) noexcept override;
};

class VisionEngine {
private:
    // Initializing TensorRT Runtime
    nvinfer1::IRuntime* runtime = nullptr;
    nvinfer1::ICudaEngine* engine = nullptr;
    nvinfer1::IExecutionContext* context = nullptr;

    // Mem Alloc
    size_t input_size;
    size_t output_size;
    
    // Mem Ptrs
    void* d_input = nullptr;
    void* d_output = nullptr;

    // CPU-side buffers
    float* h_output = nullptr;
    float* gpu_input_ptr = nullptr;
    void* buffers[2];

    Logger gLogger;

public:
    // Initializes TensorRT and allocates CUDA memory
    VisionEngine(const std::string& model_path);
    ~VisionEngine(); // Cleans up GPU memory

    // Takes a frame, returns an array of detected persons
    std::vector<Person> run_inference(cv::Mat& frame);
};
