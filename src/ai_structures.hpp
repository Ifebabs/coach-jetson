#pragma once

#include <vector>
#include <opencv2/opencv.hpp>

// Single 2D point on the body
struct Keypoint {
    float x;
    float y;
    float confidence;
};

// A detected person containing their bounding box and all 17 keypoints
struct Person {
    cv::Rect box;         // Where the person is in the frame
    float score;          // How sure the AI is this is a human
    std::vector<Keypoint> keypoints;
    
    // Helper to get a specific point by name
    Keypoint get_nose() { return keypoints[0]; }
    Keypoint get_left_wrist() { return keypoints[9]; }
    Keypoint get_right_wrist() { return keypoints[10]; }
};

