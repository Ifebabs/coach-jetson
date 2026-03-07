#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>

// Build the GStreamer pipeline for Jetson Nano CSI camera
std::string get_tegra_pipeline(int width, int height, int fps) {
    return "nvarguscamerasrc ! video/x-raw(memory:NVMM), width=(int)" + std::to_string(width) +
           ", height=(int)" + std::to_string(height) +
           ", format=(string)NV12, framerate=(fraction)" + std::to_string(fps) + "/1 ! "
           "nvvidconv flip-method=6 ! video/x-raw, width=(int)" + std::to_string(width) +
           ", height=(int)" + std::to_string(height) +
           ", format=(string)BGRx ! videoconvert ! video/x-raw, format=(string)BGR ! appsink";
}

int main() {
    int capture_width = 1280;
    int capture_height = 720;
    int framerate = 30;

    // Generate the hardware-accelerated pipeline
    std::string pipeline = get_tegra_pipeline(capture_width, capture_height, framerate);
    std::cout << "Initializing hardware pipeline:\n" << pipeline << "\n\n";

    // Open camera using GStreamer backend
    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);

    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open the CSI camera. Check ribbon cable!\n";
        return -1;
    }

    std::cout << "Camera opened! Use ESC to exit.\n";

    cv::Mat frame;

    // Vision loop
    while (true) {
        cap.read(frame);

        if (frame.empty()) {
            std::cerr << "Error: Blank frame grabbed\n";
            break;
        }

        cv::imshow("Coach Jetson - AI Vision Node", frame);

        if (cv::waitKey(1) == 27) { // ESC key
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();

    return 0;
}
