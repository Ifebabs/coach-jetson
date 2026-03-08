#pragma once

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <cmath>

// ----------------------
// CORE VISON STRUCTURES
// ----------------------
struct Keypoint { // Single 2D point on the body
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
    Keypoint get_nose() const { return keypoints[0]; }
    Keypoint get_left_wrist() const { return keypoints[9]; }
    Keypoint get_right_wrist() const { return keypoints[10]; }
};

// --------------------------
// SHADOWBOX ROUND TRACKING
// --------------------------
enum AppMode { FREE_PLAY, COUNTDOWN, ROUND_ACTIVE, SUMMARY };

struct RoundStats {
    AppMode mode = FREE_PLAY;
    int duration_seconds = 120; // Default to 2 minutes (120s)
    
    // Timers
    int64_t countdown_start_time = 0;
    int64_t round_start_time = 0;

    // Round Metrics
    int total_punches = 0;
    int jabs = 0;
    int crosses = 0;
    int combos = 0;
    int guard_drops = 0;
    
    float total_speed_mps = 0.0f;
    int total_punch_score = 0; // Score tracker
    
    // Reset function for when a new round starts
    void reset() {
        total_punches = 0; 
        jabs = 0; 
        crosses = 0; 
        combos = 0; 
        guard_drops = 0;
        total_speed_mps = 0.0f; 
        total_punch_score = 0;
    }
};

extern RoundStats current_round; // Exists in main.cpp (currently ai_coach.cpp)

// ------------------------
// UI SCALING & CONSTANTS
// ------------------------
const float UI_SCALE = 0.75f; // <-- ONLY CHANGE THIS NUMBER TO AFFECT SCALE (EX: 0.75 = 75% size)

const int HUD_X = 10;
const int HUD_Y = 10;
const int HUD_W = (int)(380 * UI_SCALE);
const int HUD_H = (int)(215 * UI_SCALE);

// Dynamic Font Sizes & Thickness
const float TXT_L = 0.6f * UI_SCALE; 
const float TXT_M = 0.55f * UI_SCALE; 
const float TXT_S = 0.5f * UI_SCALE; 

const int THICK_XL = std::max(2, (int)std::round(6.0f * UI_SCALE)); 
const int THICK_L  = std::max(1, (int)std::round(2.0f * UI_SCALE)); 
const int THICK_S  = 1;

// Dynamic Spacing
const int COL_2 = HUD_X + (int)(90 * UI_SCALE); // Feedback text starts

// Button Geometry (Adapts always below the HUD)
const int BTN_X = HUD_X;
const int BTN_W = (int)(160 * UI_SCALE);
const int BTN_H = (int)(40 * UI_SCALE);

const int START_BTN_Y = HUD_Y + HUD_H + (int)(60 * UI_SCALE); 
const int TIMER_Y = START_BTN_Y + (int)(45 * UI_SCALE);
const int END_BTN_Y = TIMER_Y + (int)(15 * UI_SCALE);


