#include "ui_render.hpp"
#include "../model/hardware_sensors.hpp" 
#include <iostream>

// ---------------------
// MOUSE CLICK LISTENER 
// ---------------------
void onMouse(int event, int x, int y, int flags, void* userdata) {
    if (event == cv::EVENT_LBUTTONDOWN) {
        
        if (current_round.mode == FREE_PLAY) {
            // Shadow Box Button
            if (x >= BTN_X && x <= BTN_X + BTN_W && y >= START_BTN_Y && y <= START_BTN_Y + BTN_H) {
                current_round.reset();
                current_round.mode = COUNTDOWN;
                current_round.countdown_start_time = cv::getTickCount();
            }
            // Reflex Drill Button
            else if (x >= BTN_X && x <= BTN_X + BTN_W && y >= REFLEX_BTN_Y && y <= REFLEX_BTN_Y + BTN_H) {
                current_round.reset();
                current_round.mode = REFLEX_ACTIVE;
                start_reflex_drill.store(true); // Tell background thread to wake up
            }
        }
        else if (current_round.mode == ROUND_ACTIVE) {
            if (x >= BTN_X && x <= BTN_X + BTN_W && y >= END_BTN_Y && y <= END_BTN_Y + BTN_H) {
                current_round.mode = SUMMARY; 
            }
        }
        // Dismiss summaries
        else if (current_round.mode == SUMMARY || current_round.mode == REFLEX_SUMMARY) {
            current_round.mode = FREE_PLAY;
        }
    }
}


// -------------------
// COACH HUD RENDERER
// -------------------
void render_hud(cv::Mat& frame, const UIData& ui) {
   // --- TOP RIGHT: ENVIRONMENT DATA ---
    char env_str[64];
    snprintf(env_str, sizeof(env_str), "Room Env: %.1f F | %.1f%%", ui.room_temp, ui.room_humidity);
    int baseline = 0;
    cv::Size text_size = cv::getTextSize(env_str, cv::FONT_HERSHEY_DUPLEX, 0.6, 1, &baseline);
    cv::putText(frame, env_str, cv::Point(frame.cols - text_size.width - 20, 30), cv::FONT_HERSHEY_DUPLEX, 0.6, cv::Scalar(200, 200, 200), 1);


    cv::Mat overlay = frame.clone();

    cv::rectangle(overlay, cv::Point(HUD_X, HUD_Y), cv::Point(HUD_X + HUD_W, HUD_Y + HUD_H), cv::Scalar(20,20,20), -1);
    cv::addWeighted(overlay, 0.45, frame, 0.55, 0, frame);

    cv::Scalar glow_color = cv::Scalar(0, 255, 255); 
    if (ui.guard_color == cv::Scalar(0, 0, 255)) glow_color = cv::Scalar(0, 0, 255); 
    else if (ui.feedback_color == cv::Scalar(0, 255, 0) || ui.cross_color == cv::Scalar(0, 255, 0)) glow_color = cv::Scalar(0, 255, 0); 
    else if (ui.feedback_color == cv::Scalar(0, 0, 255) || ui.cross_color == cv::Scalar(0, 0, 255)) glow_color = cv::Scalar(0, 0, 255); 

    cv::rectangle(frame, cv::Point(HUD_X, HUD_Y), cv::Point(HUD_X + HUD_W, HUD_Y + HUD_H), glow_color, 1);
    cv::rectangle(frame, cv::Point(HUD_X-1, HUD_Y-1), cv::Point(HUD_X + HUD_W+1, HUD_Y + HUD_H+1), glow_color, 1);

    // Pre-calculate row heights
    int r1 = HUD_Y + (int)(25 * UI_SCALE);
    int r2 = HUD_Y + (int)(55 * UI_SCALE);
    int r3 = HUD_Y + (int)(90 * UI_SCALE);
    int r4 = HUD_Y + (int)(130 * UI_SCALE);
    int r5 = HUD_Y + (int)(160 * UI_SCALE);
    int r6 = HUD_Y + (int)(190 * UI_SCALE);

    // Title
    cv::putText(frame, "AI COACH", cv::Point(HUD_X + 10, r1), cv::FONT_HERSHEY_DUPLEX, TXT_S, cv::Scalar(255,255,255), THICK_S);
    cv::line(frame, cv::Point(HUD_X + 8, r1 + 8), cv::Point(HUD_X + HUD_W - 8, r1 + 8), cv::Scalar(120,120,120), THICK_S);

    // Jabs
    cv::putText(frame, "J: " + std::to_string(ui.jab_count), cv::Point(HUD_X + 10, r2), cv::FONT_HERSHEY_SIMPLEX, TXT_L, cv::Scalar(255,255,255), THICK_L);
    char jab_info_str[64];
    snprintf(jab_info_str, sizeof(jab_info_str), "%.1f m/s | Pts: %d", ui.display_jab_speed, ui.last_jab_score);
    cv::putText(frame, ui.jab_feedback + " (" + std::string(jab_info_str) + ")", cv::Point(COL_2, r2), cv::FONT_HERSHEY_SIMPLEX, TXT_S, ui.feedback_color, THICK_S);

    // Crosses
    cv::putText(frame, "C: " + std::to_string(ui.cross_count), cv::Point(HUD_X + 10, r3), cv::FONT_HERSHEY_SIMPLEX, TXT_L, cv::Scalar(255,255,255), THICK_L);
    char cross_info_str[64];
    snprintf(cross_info_str, sizeof(cross_info_str), "%.1f m/s | Pts: %d", ui.display_cross_speed, ui.last_cross_score);
    cv::putText(frame, ui.cross_feedback + " (" + std::string(cross_info_str) + ")", cv::Point(COL_2, r3), cv::FONT_HERSHEY_SIMPLEX, TXT_S, ui.cross_color, THICK_S);

    // UI Metrics
    cv::putText(frame, ui.guard_feedback, cv::Point(HUD_X + 10, r4), cv::FONT_HERSHEY_SIMPLEX, TXT_M, ui.guard_color, THICK_L);
    cv::putText(frame, ui.balance_feedback, cv::Point(HUD_X + 10, r5), cv::FONT_HERSHEY_SIMPLEX, TXT_M, ui.balance_color, THICK_L);
    cv::putText(frame, ui.recovery_feedback, cv::Point(HUD_X + 10, r6), cv::FONT_HERSHEY_SIMPLEX, TXT_M, ui.recovery_color, THICK_L);            

    // Combo UI (Centering Horizontally)
    double time_since_combo = (cv::getTickCount() - ui.combo_display_timer) / cv::getTickFrequency();
    if (time_since_combo < 2.0 && ui.combo_feedback != "") {
        int baseline = 0;
        cv::Size text_size = cv::getTextSize(ui.combo_feedback, cv::FONT_HERSHEY_DUPLEX, TXT_L, THICK_L, &baseline);
        int center_x = HUD_X + (HUD_W / 2) - (text_size.width / 2);
        int combo_y = HUD_Y + HUD_H + (int)(25 * UI_SCALE);
        cv::putText(frame, ui.combo_feedback, cv::Point(center_x, combo_y), cv::FONT_HERSHEY_DUPLEX, TXT_L, cv::Scalar(0, 255, 255), THICK_L);
    }
}

// -----------------------
// ROUND STATE & OVERLAYS
// -----------------------
void render_round_overlays(cv::Mat& frame) {
    if (current_round.mode == FREE_PLAY) { // START ROUND Button under HUD
        cv::rectangle(frame, cv::Point(BTN_X, START_BTN_Y), cv::Point(BTN_X + BTN_W, START_BTN_Y + BTN_H), cv::Scalar(0, 150, 0), -1);
        cv::putText(frame, "START ROUND", cv::Point(BTN_X + 10, START_BTN_Y + (int)(25 * UI_SCALE)), cv::FONT_HERSHEY_SIMPLEX, TXT_L, cv::Scalar(255, 255, 255), THICK_L);
        
        // START REFLEX Button
        cv::rectangle(frame, cv::Point(BTN_X, REFLEX_BTN_Y), cv::Point(BTN_X + BTN_W, REFLEX_BTN_Y + BTN_H), cv::Scalar(255, 100, 0), -1); // Blue button
        cv::putText(frame, "REFLEX DRILL", cv::Point(BTN_X + 10, REFLEX_BTN_Y + (int)(25 * UI_SCALE)), cv::FONT_HERSHEY_SIMPLEX, TXT_L, cv::Scalar(255, 255, 255), THICK_L);

    } 
    else if (current_round.mode == COUNTDOWN) { // 3,2,1 - FIGHT Countdown
        double elapsed = (cv::getTickCount() - current_round.countdown_start_time) / cv::getTickFrequency();
        int remaining = 3 - (int)elapsed;
        int text_y = frame.rows/2 + 60; 
        
        if (remaining > 0) {
            std::string num_text = std::to_string(remaining);
            int baseline = 0;
            cv::Size text_size = cv::getTextSize(num_text, cv::FONT_HERSHEY_DUPLEX, 5.0, THICK_XL, &baseline);
            int center_x = (frame.cols / 2) - (text_size.width / 2);
            cv::putText(frame, num_text, cv::Point(center_x, text_y), cv::FONT_HERSHEY_DUPLEX, 5.0, cv::Scalar(0, 0, 255), THICK_XL);
        } else if (remaining == 0) {
            std::string fight_text = "FIGHT!";
            int baseline = 0;
            cv::Size text_size = cv::getTextSize(fight_text, cv::FONT_HERSHEY_DUPLEX, 4.0, THICK_XL, &baseline);
            int center_x = (frame.cols / 2) - (text_size.width / 2);
            cv::putText(frame, fight_text, cv::Point(center_x, text_y), cv::FONT_HERSHEY_DUPLEX, 4.0, cv::Scalar(0, 255, 0), THICK_XL);
        } else {
            current_round.mode = ROUND_ACTIVE;
            current_round.round_start_time = cv::getTickCount();
        }
    } 
    else if (current_round.mode == ROUND_ACTIVE) { // Ticking Round TImer
        double elapsed = (cv::getTickCount() - current_round.round_start_time) / cv::getTickFrequency();
        int time_left = current_round.duration_seconds - (int)elapsed;
        
        if (time_left <= 0) { // ROUND END
            current_round.mode = SUMMARY; 
        } else {
            int mins = time_left / 60;
            int secs = time_left % 60;
            char time_str[16];
            snprintf(time_str, sizeof(time_str), "%02d:%02d", mins, secs);
            cv::putText(frame, time_str, cv::Point(BTN_X, TIMER_Y), cv::FONT_HERSHEY_DUPLEX, 1.2 * UI_SCALE, cv::Scalar(255, 255, 255), THICK_L);
            cv::rectangle(frame, cv::Point(BTN_X, END_BTN_Y), cv::Point(BTN_X + BTN_W, END_BTN_Y + BTN_H), cv::Scalar(0, 0, 150), -1);
            cv::putText(frame, "END ROUND", cv::Point(BTN_X + 15, END_BTN_Y + (int)(25 * UI_SCALE)), cv::FONT_HERSHEY_SIMPLEX, TXT_L, cv::Scalar(255, 255, 255), THICK_L);
        }
    } 
    
    // --- REFLEX ACTIVE UI ---
    else if (current_round.mode == REFLEX_ACTIVE) {
        // Check if the hardware thread finished the drill
        if (drill_completed.load()) {
            current_round.mode = REFLEX_SUMMARY;
        } else {
            // Draw Drill Status below HUD
            int r = current_drill_round.load();
            cv::putText(frame, "Drill: Round " + std::to_string(r) + " / 5", cv::Point(BTN_X, TIMER_Y), cv::FONT_HERSHEY_DUPLEX, 1.0 * UI_SCALE, cv::Scalar(255, 255, 255), THICK_L);
            cv::putText(frame, "Last Reaction: " + std::to_string(last_reaction_time.load()) + " ms", cv::Point(BTN_X, TIMER_Y + 30), cv::FONT_HERSHEY_SIMPLEX, TXT_L, cv::Scalar(0, 165, 255), THICK_L);

            // Draw Huge Screen Center Text
            int text_y = frame.rows / 2 + 50;
            if (show_punch_cue.load()) {
                std::string cue = "PUNCH NOW!";
                int b = 0;
                cv::Size ts = cv::getTextSize(cue, cv::FONT_HERSHEY_DUPLEX, 3.5, THICK_XL, &b);
                cv::putText(frame, cue, cv::Point((frame.cols/2) - (ts.width/2), text_y), cv::FONT_HERSHEY_DUPLEX, 3.5, cv::Scalar(0, 0, 255), THICK_XL); // Red
            } else {
                std::string wait = "Wait for it...";
                int b = 0;
                cv::Size ts = cv::getTextSize(wait, cv::FONT_HERSHEY_DUPLEX, 2.0, THICK_L, &b);
                cv::putText(frame, wait, cv::Point((frame.cols/2) - (ts.width/2), text_y), cv::FONT_HERSHEY_DUPLEX, 2.0, cv::Scalar(0, 255, 255), THICK_L); // Yellow
            }
        }
    }
    
    // --- REFLEX SUMMARY UI ---
    else if (current_round.mode == REFLEX_SUMMARY) {
        cv::Mat summary_overlay = frame.clone();
        cv::rectangle(summary_overlay, cv::Point(0,0), cv::Point(frame.cols, frame.rows), cv::Scalar(30, 30, 30), -1);
        cv::addWeighted(summary_overlay, 0.85, frame, 0.15, 0, frame);

        int cx = frame.cols/2 - 180;
        int cy = frame.rows/2 - 50; 
        cv::putText(frame, "DRILL COMPLETE", cv::Point(cx, cy), cv::FONT_HERSHEY_DUPLEX, 1.2, cv::Scalar(255, 255, 255), 2);
        cv::line(frame, cv::Point(cx, cy + 15), cv::Point(cx + 350, cy + 15), cv::Scalar(100, 100, 100), 2);
        
        cv::putText(frame, "Avg Reaction: " + std::to_string(avg_reaction_time.load()) + " ms", cv::Point(cx, cy + 60), cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(0, 165, 255), 2);
        cv::putText(frame, "(Click anywhere to close)", cv::Point(cx + 30, cy + 120), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(150, 150, 150), 1);
    }

    else if (current_round.mode == SUMMARY) { // Overlay Screen
        cv::Mat summary_overlay = frame.clone();
        cv::rectangle(summary_overlay, cv::Point(0,0), cv::Point(frame.cols, frame.rows), cv::Scalar(30, 30, 30), -1);
        cv::addWeighted(summary_overlay, 0.85, frame, 0.15, 0, frame);

        
        // Format Averages
        float avg_spd = current_round.total_punches > 0 ? (current_round.total_speed_mps / current_round.total_punches) : 0.0f;
        int avg_score = current_round.total_punches > 0 ? (current_round.total_punch_score / current_round.total_punches) : 0;
        
        char spd_str[64]; 
        snprintf(spd_str, sizeof(spd_str), "Avg Punch Speed: %.1f m/s", avg_spd);
        char score_str[64];
        snprintf(score_str, sizeof(score_str), "Avg Punch Score: %d / 100", avg_score);
    

        // Stats Text
        int cx = frame.cols/2 - 180;
        int cy = frame.rows/2 - 140; 
        cv::putText(frame, "ROUND SUMMARY", cv::Point(cx, cy), cv::FONT_HERSHEY_DUPLEX, 1.2, cv::Scalar(255, 255, 255), 2);
        cv::line(frame, cv::Point(cx, cy + 15), cv::Point(cx + 350, cy + 15), cv::Scalar(100, 100, 100), 2);
        cv::putText(frame, "Total Punches: " + std::to_string(current_round.total_punches), cv::Point(cx, cy + 50), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
        cv::putText(frame, "Jabs: " + std::to_string(current_round.jabs), cv::Point(cx, cy + 85), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
        cv::putText(frame, "Crosses: " + std::to_string(current_round.crosses), cv::Point(cx, cy + 120), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
        cv::putText(frame, "Combos Landed: " + std::to_string(current_round.combos), cv::Point(cx, cy + 155), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
        cv::putText(frame, spd_str, cv::Point(cx, cy + 190), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
        cv::putText(frame, score_str, cv::Point(cx, cy + 225), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2); 
        cv::putText(frame, "Guard Drops: " + std::to_string(current_round.guard_drops), cv::Point(cx, cy + 260), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2); 
        cv::putText(frame, "(Click anywhere to close)", cv::Point(cx + 30, cy + 310), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(150, 150, 150), 1);
    }
}
