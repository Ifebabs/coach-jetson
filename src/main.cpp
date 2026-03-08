#include <iostream>
#include <fstream>
#include <cuda_runtime_api.h>
#include <opencv2/opencv.hpp>
#include "model/ai_structures.hpp"
#include "view/ui_render.hpp"
#include "viewmodel/boxing_logic.hpp"
#include "model/vision_engine.hpp"

// Round Timer Memory
RoundStats current_round;

int main() {
    
    // Loads TensorRT models (Initialize Vision Engine)
    VisionEngine pi_eyes("models/yolov8n-pose.engine");
    // Initialize Boxing Logic
    BoxingAnalyzer logic_brain;

    // Prepare Camera
    std::string pipeline = "nvarguscamerasrc ! video/x-raw(memory:NVMM), width=1280, height=720, format=NV12, framerate=30/1 ! nvvidconv flip-method=6 left=280 right=1000 top=0 bottom=720 ! video/x-raw, width=640, height=640, format=BGRx ! videoconvert ! video/x-raw, format=BGR ! appsink drop=true max-buffers=1";
    
    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera!" << std::endl;
        return -1;
    }

    cv::namedWindow("Coach Jetson - AI Boxing Coach", cv::WINDOW_NORMAL);
    cv::resizeWindow("Coach Jetson - AI Boxing Coach", 1200, 1200);
    cv::setMouseCallback("Coach Jetson - AI Boxing Coach", onMouse, nullptr); // attaches mouse listener to window

    cv::Mat frame;

    // VISION LOOP 
    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        // Run Inference (The Eyes)
        std::vector<Person> detected_persons = pi_eyes.run_inference(frame);

        // Process Skeleton, Analyze Physics
        for (const Person& person : detected_persons) {
            // Pass data to ViewModel layer
            logic_brain.analyze_frame(person.keypoints, frame);
        }
        
        // --- PACK DATA AND SEND TO VIEW LAYER ---
        UIData ui_state = logic_brain.get_ui_data();

        // --- DRAW EVERYTHING ---
        render_hud(frame, ui_state);
        render_round_overlays(frame);

        // Output
        cv::imshow("Coach Jetson - AI Boxing Coach", frame);

		// Break if ESC OR if the window is closed
        if (cv::waitKey(1) == 27 || cv::getWindowProperty("Coach Jetson - AI Boxing Coach", cv::WND_PROP_AUTOSIZE) == -1) {
            break;
		}
    }

    return 0; // Vision Engine destructor cleans CPU
}
