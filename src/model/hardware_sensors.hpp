#pragma once
#include <atomic>

// Environment Variables
extern std::atomic<float> current_temp;
extern std::atomic<float> current_humidity;

// Reflex Drill Variables
extern std::atomic<long long> last_reaction_time;
extern std::atomic<int> current_wave_size;

// Synchronization Flags
extern std::atomic<bool> start_reflex_drill;
extern std::atomic<bool> drill_completed;
extern std::atomic<bool> show_punch_cue;
extern std::atomic<int> current_drill_round;
extern std::atomic<long long> avg_reaction_time;

void bme280_polling_thread();
void mcp3008_reflex_thread();
