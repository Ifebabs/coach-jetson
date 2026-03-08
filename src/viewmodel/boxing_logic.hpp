#pragma once

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include "../model/ai_structures.hpp"
#include "../view/ui_render.hpp" 

// Function to calculate the interior angle between three keypoints (e.g., Shoulder, Elbow, Wrist)
float calculate_angle(Keypoint a, Keypoint b, Keypoint c);

class BoxingAnalyzer {
private:
    // ----------------------------
    // --- STATE MACHINE MEMORY ---
    // ----------------------------
    int jab_count = 0;
    bool is_jab_extended = false;
    float max_jab_angle = 0.0f;
    std::string jab_feedback = "READY";
    cv::Scalar feedback_color = cv::Scalar(255, 255, 255);

    int cross_count = 0;
    bool is_cross_extended = false;
    float max_cross_angle = 0.0f;
    std::string cross_feedback = "READY";
    cv::Scalar cross_color = cv::Scalar(255, 255, 255);

    std::string guard_feedback = "GOOD GUARD!";
    cv::Scalar guard_color = cv::Scalar(0, 255, 0);
    bool was_guard_dropped = false;

    // Stance Detection was considered but there is not enough depth perception
    // and the camera angle cannot reliably provide data for this measurement

    std::string balance_feedback = "GOOD BALANCE!";
    cv::Scalar balance_color = cv::Scalar(0, 255, 0);

    // Real-Time Stopwatch Variables for Speed
    int64_t jab_start_time = 0;
    cv::Point2f jab_start_wrist;
    float display_jab_speed = 0.0f;

    int64_t cross_start_time = 0;
    cv::Point2f cross_start_wrist;
    float display_cross_speed = 0.0f;

    // Combo Recognition Variables
    int64_t last_punch_time = 0;
    std::string last_punch_type = "NONE";
    int current_combo_streak = 0;
    std::string combo_feedback = "";
    int64_t combo_display_timer = 0;

    // Guard Recovery Speed Variables
    int64_t jab_max_ext_time = 0;
    int64_t cross_max_ext_time = 0;
    std::string recovery_feedback = "GOOD RECOVERY!";
    cv::Scalar recovery_color = cv::Scalar(255, 255, 255);

    // Punch Scoring Variables
    int last_jab_score = 0;
    int last_cross_score = 0;

public:
    BoxingAnalyzer() : jab_start_wrist(0,0), cross_start_wrist(0,0) {}

    // Analyze skeleton and draw the lines/angles on the frame
    void analyze_frame(const std::vector<Keypoint>& kpts, cv::Mat& frame);

    // Package everything for the UI Renderer
    UIData get_ui_data() const;
};
