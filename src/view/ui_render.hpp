#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include "../model/ai_structures.hpp"

// Hands data to view layer
struct UIData {
    int jab_count, cross_count;
    float display_jab_speed, display_cross_speed;
    int last_jab_score, last_cross_score;
    
    std::string jab_feedback, cross_feedback;
    cv::Scalar feedback_color, cross_color;
    
    std::string guard_feedback, balance_feedback, recovery_feedback;
    cv::Scalar guard_color, balance_color, recovery_color;
    
    std::string combo_feedback;
    int64_t combo_display_timer;
    
    // Hardware Variables 
    float room_temp = 0.0f;
    float room_humidity = 0.0f;

};

// UI functions
void onMouse(int event, int x, int y, int flags, void* userdata);
void render_hud(cv::Mat& frame, const UIData& ui);
void render_round_overlays(cv::Mat& frame);
