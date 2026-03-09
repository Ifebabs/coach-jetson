#include "boxing_logic.hpp"
#include <cmath>
#include <algorithm>

// Function to calculate the interior angle between three keypoints (e.g., Shoulder, Elbow, Wrist)
float calculate_angle(Keypoint a, Keypoint b, Keypoint c) {
    float radians = atan2(c.y - b.y, c.x - b.x) - atan2(a.y - b.y, a.x - b.x); // Radian Angle Calculation
    float angle = std::abs(radians * 180.0f / CV_PI); // Convert to degrees
    // Normalize the angle so it's always between 0 and 180 degrees
    if (angle > 180.0f) angle = 360.0f - angle;
    return angle;
}

void BoxingAnalyzer::analyze_frame(const std::vector<Keypoint>& kpts, cv::Mat& frame) {
    // Draws green skeleton dots
    /*
    for(int k = 0; k < 17; k++) {
        if(kpts[k].confidence > 0.5f) {
            cv::circle(frame, cv::Point(kpts[k].x, kpts[k].y), 4, cv::Scalar(0, 255, 0), -1);
        }
    }
    */
    // --- ACTUAL LEFT ARM (MIRRORED RIGHT INDICES: 6, 8, 10) ---
    if (kpts[6].confidence > 0.5f && kpts[8].confidence > 0.5f && kpts[10].confidence > 0.5f) {
        // Blue line for Left Arm (Jab)
        cv::line(frame, cv::Point(kpts[6].x, kpts[6].y), cv::Point(kpts[8].x, kpts[8].y), cv::Scalar(255, 0, 0), 2);
        cv::line(frame, cv::Point(kpts[8].x, kpts[8].y), cv::Point(kpts[10].x, kpts[10].y), cv::Scalar(255, 0, 0), 2);
        
        float left_elbow_angle = calculate_angle(kpts[6], kpts[8], kpts[10]);
        cv::Point2f current_left_wrist(kpts[10].x, kpts[10].y);

        // JAB STATE MACHINE
        if (!is_jab_extended) {
            // Punch Initiated
            if (left_elbow_angle > 120.0f && kpts[10].y < (kpts[6].y + 80.0f)) {
                jab_count++;
                is_jab_extended = true;
                max_jab_angle = left_elbow_angle;

                if (current_round.mode == ROUND_ACTIVE) {
                    current_round.jabs++;
                    current_round.total_punches++;
                }
                
                jab_start_time = cv::getTickCount();
                jab_start_wrist = current_left_wrist;

                 // --- COMBO LOGIC (JAB) ---
                double time_since_last = (cv::getTickCount() - last_punch_time) / cv::getTickFrequency();
                if (time_since_last < 1.0) {
                    current_combo_streak++; 
                    if (current_combo_streak == 2) {
                        if (current_round.mode == ROUND_ACTIVE) current_round.combos++; 
                        if (last_punch_type == "JAB") combo_feedback = "DOUBLE JAB!";
                        else if (last_punch_type == "CROSS") combo_feedback = "2-1 COMBO!";
                    } 
                    else if (current_combo_streak == 3) combo_feedback = "3-PIECE COMBO!";
                    else if (current_combo_streak > 3) combo_feedback = std::to_string(current_combo_streak) + "-PUNCH COMBO!!!";
                    combo_display_timer = cv::getTickCount();
                } else {
                    current_combo_streak = 1; // Too slow, reset streak
                }
                last_punch_type = "JAB"; 
                last_punch_time = cv::getTickCount();
            }
        } else {
            // Punch is extending
            if (left_elbow_angle > max_jab_angle) {
                max_jab_angle = left_elbow_angle;
                jab_max_ext_time = cv::getTickCount(); // Peak of punch

                // Calculate speed of OUTWARD motion using CPU time
                double time_elapsed = (cv::getTickCount() - jab_start_time) / cv::getTickFrequency();
                if (time_elapsed > 0.05) { 
                    float pixel_distance = cv::norm(current_left_wrist - jab_start_wrist);
                    if (kpts[5].confidence > 0.5f && kpts[6].confidence > 0.5f) {
                        float shoulder_width_px = std::abs(kpts[6].x - kpts[5].x);
                        if (shoulder_width_px > 30.0f) { 
                            float meters_per_pixel = 0.41f / shoulder_width_px;
                            display_jab_speed = (pixel_distance / time_elapsed) * meters_per_pixel * 1.5f;
                        }
                    }
                }
            }
            
            // Live Grading
            if (max_jab_angle >= 160.0f) {
                jab_feedback = "GOOD JAB!";
                feedback_color = cv::Scalar(0, 255, 0);
            } else {
                jab_feedback = "EXTEND FULLY!";
                feedback_color = cv::Scalar(0, 0, 255);
            }

            // The Reset (Arm returning to guard) 
            if (left_elbow_angle < 100.0f) {
                // Time it took to pull hand back
                double recovery_time = (cv::getTickCount() - jab_max_ext_time) / cv::getTickFrequency();

                // QUALITY SCORING ALGORITHM (0-100)
                float ext_score = std::max(0.0f, std::min(40.0f, ((max_jab_angle - 120.0f) / 40.0f) * 40.0f));
                float spd_score = std::min(40.0f, (display_jab_speed / 6.0f) * 40.0f);
                float rec_score = 20.0f;
                if (recovery_time > 0.6) rec_score -= (recovery_time - 0.6f) * 20.0f; 
                rec_score = std::max(0.0f, rec_score);
                
                last_jab_score = (int)(ext_score + spd_score + rec_score);

                // Add speed and score to the Round Bank
                if (current_round.mode == ROUND_ACTIVE) {
                    current_round.total_speed_mps += display_jab_speed;
                    current_round.total_punch_score += last_jab_score;
                }

                if (recovery_time > 1.1) { 
                    recovery_feedback = "RETRACT FASTER!";
                    recovery_color = cv::Scalar(0, 0, 255); 
                } else if (recovery_time > 0.7) {
                    recovery_feedback = "SNAP IT BACK!";
                    recovery_color = cv::Scalar(0, 165, 255); 
                } else {
                    recovery_feedback = "GOOD RECOVERY!";
                    recovery_color = cv::Scalar(0, 255, 0); 
                }
                
                // Reset State Machine to READY
                is_jab_extended = false;
                max_jab_angle = 0.0f;
                jab_feedback = "READY";
                feedback_color = cv::Scalar(255, 255, 255);
            }
        }
        cv::putText(frame, std::to_string((int)left_elbow_angle) + " deg", cv::Point(kpts[8].x + 15, kpts[8].y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2);
    }

    // --- ACTUAL RIGHT ARM (MIRRORED LEFT INDICES: 5, 7, 9) ---
    // Red line for Right Arm (Cross)
    if (kpts[5].confidence > 0.5f && kpts[7].confidence > 0.5f && kpts[9].confidence > 0.5f) {
        cv::line(frame, cv::Point(kpts[5].x, kpts[5].y), cv::Point(kpts[7].x, kpts[7].y), cv::Scalar(0, 0, 255), 2);
        cv::line(frame, cv::Point(kpts[7].x, kpts[7].y), cv::Point(kpts[9].x, kpts[9].y), cv::Scalar(0, 0, 255), 2);
        
        float right_elbow_angle = calculate_angle(kpts[5], kpts[7], kpts[9]);
        cv::Point2f current_right_wrist(kpts[9].x, kpts[9].y);

        // CROSS STATE MACHINE
        if (!is_cross_extended) {
            if (right_elbow_angle > 120.0f && kpts[9].y < (kpts[5].y + 80.0f)) {
                cross_count++;
                is_cross_extended = true;
                max_cross_angle = right_elbow_angle;

                if (current_round.mode == ROUND_ACTIVE) {
                    current_round.crosses++;
                    current_round.total_punches++;
                }
                
                cross_start_time = cv::getTickCount();
                cross_start_wrist = current_right_wrist;

                // --- COMBO LOGIC (CROSS) ---
                double time_since_last = (cv::getTickCount() - last_punch_time) / cv::getTickFrequency();
                if (time_since_last < 1.0) {
                    current_combo_streak++; 
                    if (current_combo_streak == 2) {
                        if (current_round.mode == ROUND_ACTIVE) current_round.combos++; 
                        if (last_punch_type == "JAB") combo_feedback = "1-2 COMBO!";
                        else if (last_punch_type == "CROSS") combo_feedback = "DOUBLE CROSS!";
                    } 
                    else if (current_combo_streak == 3) combo_feedback = "3-PIECE COMBO!";
                    else if (current_combo_streak > 3) combo_feedback = std::to_string(current_combo_streak) + "-PUNCH COMBO!!!";
                    combo_display_timer = cv::getTickCount();
                } else {
                    current_combo_streak = 1; // Too slow, reset streak
                }
                last_punch_type = "CROSS"; 
                last_punch_time = cv::getTickCount();
            }
        } else {
            // Punch is extending
            if (right_elbow_angle > max_cross_angle) {
                max_cross_angle = right_elbow_angle;
                cross_max_ext_time = cv::getTickCount(); // Peak of punch
                
                double time_elapsed = (cv::getTickCount() - cross_start_time) / cv::getTickFrequency();
                if (time_elapsed > 0.05) { 
                    float pixel_distance = cv::norm(current_right_wrist - cross_start_wrist);
                    if (kpts[5].confidence > 0.5f && kpts[6].confidence > 0.5f) {
                        float shoulder_width_px = std::abs(kpts[6].x - kpts[5].x);
                        if (shoulder_width_px > 30.0f) { 
                            float meters_per_pixel = 0.41f / shoulder_width_px;
                            display_cross_speed = (pixel_distance / time_elapsed) * meters_per_pixel * 1.5f;
                        }
                    }
                }
            }
            
            if (max_cross_angle >= 160.0f) {
                cross_feedback = "GOOD CROSS!";
                cross_color = cv::Scalar(0, 255, 0); 
            } else {
                cross_feedback = "EXTEND FULLY!";
                cross_color = cv::Scalar(0, 0, 255); 
            }

            // The Reset (Arm returning to guard) 
            if (right_elbow_angle < 100.0f) {
                double recovery_time = (cv::getTickCount() - cross_max_ext_time) / cv::getTickFrequency();

                // QUALITY SCORING ALGORITHM (0-100)
                float ext_score = std::max(0.0f, std::min(40.0f, ((max_cross_angle - 120.0f) / 40.0f) * 40.0f));
                float spd_score = std::min(40.0f, (display_cross_speed / 6.0f) * 40.0f);
                float rec_score = 20.0f;
                if (recovery_time > 0.8) rec_score -= (recovery_time - 0.8f) * 20.0f; 
                rec_score = std::max(0.0f, rec_score);
                
                last_cross_score = (int)(ext_score + spd_score + rec_score);

                // Add the speed and score to the Round Bank
                if (current_round.mode == ROUND_ACTIVE) {
                    current_round.total_speed_mps += display_cross_speed;
                    current_round.total_punch_score += last_cross_score;
                }

                if (recovery_time > 1.3) { 
                    recovery_feedback = "RETRACT FASTER!";
                    recovery_color = cv::Scalar(0, 0, 255); 
                } else if (recovery_time > 0.8) {
                    recovery_feedback = "SNAP IT BACK!";
                    recovery_color = cv::Scalar(0, 165, 255); 
                } else {
                    recovery_feedback = "GOOD RECOVERY!";
                    recovery_color = cv::Scalar(0, 255, 0); 
                }

                is_cross_extended = false;
                max_cross_angle = 0.0f;
                cross_feedback = "READY";
                cross_color = cv::Scalar(255, 255, 255); 
            }
        }
        cv::putText(frame, std::to_string((int)right_elbow_angle) + " deg", cv::Point(kpts[7].x + 15, kpts[7].y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2);
    }
    
    // --- GUARD DETECTION LOGIC ---
    // Need Nose[0] to evaluate guard
    if (kpts[0].confidence > 0.5f) {
        bool left_guard_dropped = false;
        bool right_guard_dropped = false;

        // Check Left Hand (10) ONLY if it is not currently throwing a Jab
        if (!is_jab_extended && kpts[10].confidence > 0.5f) {
            if (kpts[10].y > (kpts[0].y + 200.0f)) left_guard_dropped = true;
        }

         // Check Right Hand (9) ONLY if it is not currently throwing a Cross
        if (!is_cross_extended && kpts[9].confidence > 0.5f) {
            if (kpts[9].y > (kpts[0].y + 200.0f)) right_guard_dropped = true;
        }

        // Live Feedback & Round Tracking
        if (left_guard_dropped || right_guard_dropped) {
            guard_feedback = "RAISE YOUR GUARD!";
            guard_color = cv::Scalar(0, 0, 255); 
            // Guard Drop Latch
            if (!was_guard_dropped && current_round.mode == ROUND_ACTIVE) {
                current_round.guard_drops++;
                was_guard_dropped = true;
            }
        } else {
            guard_feedback = "GUARD: GOOD";
            guard_color = cv::Scalar(0, 255, 0); 
            was_guard_dropped = false; // Reset the latch when hands are back up
        }
    }
    
    // --- BALANCE DETECTION LOGIC ---
    // Ensure Nose (0), Actual Right Hip (11), and Actual Left Hip (12) are visible
    if (kpts[0].confidence > 0.5f && kpts[11].confidence > 0.5f && kpts[12].confidence > 0.5f) {

        // Calculate the Center of Mass (Midpoint of the hips on the X-axis)
        float hip_center_x = (kpts[11].x + kpts[12].x) / 2.0f;
        float head_x = kpts[0].x;

        // Measure how far left or right the head is drifting from the hips
        float lean_distance = std::abs(head_x - hip_center_x);
        
        // Off balance if head leans more than 100 pixels off center line
        if (lean_distance > 100.0f) {
            balance_feedback = "BALANCE: LEANING!";
            balance_color = cv::Scalar(0, 165, 255); 
        } else {
            balance_feedback = "GOOD BALANCE!";
            balance_color = cv::Scalar(0, 255, 0); 
        }

        // Can potentially remove this:
        // Line from hip center to the nose to visualize the balance axis
        // cv::line(frame, cv::Point(hip_center_x, (kpts[11].y + kpts[12].y)/2.0f), cv::Point(head_x, kpts[0].y), cv::Scalar(255, 255, 0), 1);
    }
}

UIData BoxingAnalyzer::get_ui_data() const {
    UIData ui;
    ui.jab_count = jab_count;
    ui.cross_count = cross_count;
    ui.display_jab_speed = display_jab_speed;
    ui.display_cross_speed = display_cross_speed;
    ui.last_jab_score = last_jab_score;
    ui.last_cross_score = last_cross_score;
    ui.jab_feedback = jab_feedback;
    ui.cross_feedback = cross_feedback;
    ui.feedback_color = feedback_color;
    ui.cross_color = cross_color;
    ui.guard_feedback = guard_feedback;
    ui.guard_color = guard_color;
    ui.balance_feedback = balance_feedback;
    ui.balance_color = balance_color;
    ui.recovery_feedback = recovery_feedback;
    ui.recovery_color = recovery_color;
    ui.combo_feedback = combo_feedback;
    ui.combo_display_timer = combo_display_timer;
    return ui;
}
