#include "vision_engine.hpp"
#include <iostream>
#include <fstream>

// TensorRT Logger Impl
void Logger::log(Severity severity, const char* msg) noexcept {
    if (severity <= Severity::kWARNING) std::cout << "[TRT] " << msg << std::endl;
}

// Initialization
VisionEngine::VisionEngine(const std::string& model_path) {
    std::ifstream file(model_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "Error: Could not find engine file!" << std::endl;
        exit(-1);
    }

    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0, file.beg);
    char* engine_data = new char[size];
    file.read(engine_data, size);
    file.close();

    // Initialize TensorRT Runtime
    runtime = nvinfer1::createInferRuntime(gLogger);
    engine = runtime->deserializeCudaEngine(engine_data, size);
    context = engine->createExecutionContext();
    delete[] engine_data;

    std::cout << " AI Engine Loaded Successfully on Jetson GPU!" << std::endl;

    // Allocate Memory ONCE
    input_size = 1 * 3 * 640 * 640 * sizeof(float);
    output_size = 1 * 56 * 8400 * sizeof(float);

    cudaMalloc(&d_input, input_size);
    cudaMalloc(&d_output, output_size);

    h_output = new float[56 * 8400];
    gpu_input_ptr = new float[3 * 640 * 640]; 

    buffers[0] = d_input;
    buffers[1] = d_output;
}

// CLEANUP 
VisionEngine::~VisionEngine() {
    delete[] gpu_input_ptr;
    delete[] h_output;
    cudaFree(d_input);
    cudaFree(d_output);
    delete context;
    delete engine;
    delete runtime;
}

// --- INFERENCE LOOP ---
std::vector<Person> VisionEngine::run_inference(cv::Mat& frame) {

    // Preprocess/Normalize
    cv::Mat blob;
    cv::cvtColor(frame, blob, cv::COLOR_BGR2RGB); 
    blob.convertTo(blob, CV_32FC3, 1.0f / 255.0f);

    std::vector<cv::Mat> channels(3);
    cv::split(blob, channels);
    for (int i = 0; i < 3; ++i) {
        // Overwrite same memory space, avoiding malloc/free
        memcpy(gpu_input_ptr + (i * 640 * 640), channels[i].data, 640 * 640 * sizeof(float));
    }

    // Push to GPU, Execute Inference, Pull results back to CPU
    cudaMemcpy(d_input, gpu_input_ptr, input_size, cudaMemcpyHostToDevice);
    context->enqueueV2(buffers, 0, nullptr);
    cudaMemcpy(h_output, d_output, output_size, cudaMemcpyDeviceToHost);

    // Post-Processing (8,400 predictions)
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<std::vector<Keypoint>> all_keypoints;

    for (int col = 0; col < 8400; ++col) {
        float score = h_output[4 * 8400 + col]; // Confidence score in row 4
        
        if (score > 0.5f) { // Execute when AI is > 50% sure it's a person
            float cx = h_output[0 * 8400 + col];
            float cy = h_output[1 * 8400 + col];
            float w  = h_output[2 * 8400 + col];
            float h  = h_output[3 * 8400 + col];

            int left = int(cx - w / 2);
            int top = int(cy - h / 2);

            boxes.push_back(cv::Rect(left, top, int(w), int(h)));
            scores.push_back(score);
            
            // Extract all 17 Keypoints for this person
            std::vector<Keypoint> kpts;
            for (int k = 0; k < 17; ++k) {
                float kx = h_output[(5 + k * 3) * 8400 + col];
                float ky = h_output[(5 + k * 3 + 1) * 8400 + col];
                float kconf = h_output[(5 + k * 3 + 2) * 8400 + col];
                kpts.push_back({kx, ky, kconf});
            }
            all_keypoints.push_back(kpts);
        }
    }

    // Non-Maximum Suppression (NMS)
    // AI often guesses the same person 5 or 6 times.
    // This function deletes the overlapping boxes and keeps the best one
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, scores, 0.5f, 0.4f, indices);

    // Package results into our Person struct
    std::vector<Person> detected_persons;
    for (int idx : indices) {
        Person p;
        p.box = boxes[idx];
        p.score = scores[idx];
        p.keypoints = all_keypoints[idx];
        detected_persons.push_back(p);
    }

    return detected_persons;
}
