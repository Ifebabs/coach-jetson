#include "hardware_sensors.hpp"

#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <linux/i2c-dev.h>
#include <sys/time.h>
#include <time.h>

#define CALIB_VAL 800 // MCP3008 Threshold

// --- Global Atomics for UI Rendering ---
std::atomic<float> current_temp(0.0f);
std::atomic<float> current_humidity(0.0f);
std::atomic<long long> last_reaction_time(0);
std::atomic<int> current_wave_size(0);
std::atomic<bool> start_reflex_drill(false);
std::atomic<bool> drill_completed(false);
std::atomic<bool> show_punch_cue(false);
std::atomic<int> current_drill_round(0);
std::atomic<long long> avg_reaction_time(0);


// ============================================================================
// BME280 I2C ENVIRONMENT SENSOR LOGIC
// ============================================================================

// Calibration variables (Bosch Data sheet)
static int32_t t_fine;
static uint16_t dig_T1; 
static int16_t dig_T2, dig_T3;
// static uint16_t dig_P1; 
// static int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t dig_H1, dig_H3; 
static int16_t dig_H2, dig_H4, dig_H5; 
static int8_t dig_H6;

static void read_bytes(int fd, uint8_t reg, uint8_t *data, int length) {
    if (write(fd, &reg, 1) != 1) return;
    if (read(fd, data, length) != length) return;
}

static void write_byte(int fd, uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    if (write(fd, buf, 2) != 2){
        std::cerr << "[BME280] Write failed" << std::endl;
    }
}

void bme280_polling_thread() {
    int fd;
    const char *filename = "/dev/i2c-1";
    int addr = 0x76; // BME280 I2C addr

    if ((fd = open(filename, O_RDWR)) < 0) {
        std::cerr << "[BME280] Failed to open I2C bus" << std::endl;
        return;
    }
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        std::cerr << "[BME280] Failed to connect to sensor" << std::endl;
        close(fd);
        return;
    }

    // --- Read Calibration Data ---
    uint8_t calib[32];
    read_bytes(fd, 0x88, calib, 24); // Temp and Pressure calibration
    read_bytes(fd, 0xE1, calib + 24, 7); // Humidity calibration (+ 24 lets both reads land in the same buffer cleanly)
    
    // Unpack Temperature Calibration
    dig_T1 = (calib[1] << 8) | calib[0];
    dig_T2 = (calib[3] << 8) | calib[2];
    dig_T3 = (calib[5] << 8) | calib[4];

    // Unpack Humidity Calibration (A bit scattered in memory)
    read_bytes(fd, 0xA1, &dig_H1, 1);
    dig_H2 = (calib[25] << 8) | calib[24];
    dig_H3 = calib[26];
    dig_H4 = (calib[27] << 4) | (calib[28] & 0x0F); 
    dig_H5 = (calib[29] << 4) | (calib[28] >> 4); 
    dig_H6 = calib[30];

    while (true) {
        // --- Configure and Trigger Sensor ---
        // Must set ctrl_hum (0xF2) before ctrl_meas (0xF4) or humidity setting will be ignored
        write_byte(fd, 0xF2, 0x01); // Humidity oversampling x1
        write_byte(fd, 0xF4, 0x25); // Temperature/Pressure oversampling x1, Forced mode (takes 1 reading and sleeps)
    
        usleep(50000); // Wait 50ms for the measurement to complete (datasheet requires 10ms)

        // --- Read raw ADC data ---
        uint8_t data[8];
        read_bytes(fd, 0xF7, data, 8);

        // analog to digital converter raw output 
        int32_t adc_T = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
        int32_t adc_H = (data[6] << 8) | data[7];

        // --- Bosch Compensate Temperature Algorithm ---
        // Taken from BME280 datasheet:
        // Bosch designed this to work on microcontrollers without floating point units (dedicated hardware coprocessor designed to efficiently perform mathematical calculations on numbers with decimal points)
        int32_t var1, var2, T;
        var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
        var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
        t_fine = var1 + var2;
        T = (t_fine * 5 + 128) >> 8;
        float temp_celsius = T / 100.0f;
        float temp_fahrenheit = (temp_celsius * 9.0f / 5.0f) + 32.0f;

        // --- Bosch Compensate Humidity Algorithm ---
        // Also taken from BME280 datasheet:
        int32_t v_x1_u32r;
        v_x1_u32r = (t_fine - ((int32_t)76800));
        v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)dig_H2) + 8192) >> 14));
        v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
        v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
        v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
        float humidity = (uint32_t)(v_x1_u32r >> 12) / 1024.0f;

        // STORE DATA FOR UI
        current_temp.store(temp_fahrenheit);
        current_humidity.store(humidity);

        // Sleep to prevent locking up the I2C bus and CPU
        sleep(2); 
    }

    close(fd);
}

// ============================================================================
// MCP3008 SPI REFLEX SENSOR LOGIC
// ============================================================================

static int read_adc(int fd, int channel) {
    uint8_t tx[] = { 1, (uint8_t)((8 + channel) << 4), 0 };
    uint8_t rx[3] = {0};

    struct spi_ioc_transfer tr = {};
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = 3;
    tr.speed_hz = 1000000;
    tr.bits_per_word = 8;
    
    ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    
    return ((rx[1] & 3) << 8) + rx[2];
}

// Function to get the current time in milliseconds
static long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL);
    return te.tv_sec * 1000LL + te.tv_usec / 1000;
}

void mcp3008_reflex_thread() {
    int fd = open("/dev/spidev0.0", O_RDWR);
    if (fd < 0) {
        std::cerr << "[MCP3008] Error opening SPI device" << std::endl;
        return;
    }

    // Seed the random number generator
    srand(time(NULL));

    while (true) {
        // Idle state: wait for UI button press
        if (!start_reflex_drill.load()) {
            usleep(100000); // sleep for 100ms so we don't burn CPU while waiting
            continue;
        }

        // Drill Started!
        drill_completed.store(false);
        long long total_reaction_time = 0;

        int rounds = 5; // 5-punch drill

        for (int r = 1; r <= rounds; r++) {
            current_drill_round.store(r);
            show_punch_cue.store(false);

            // Random delay between 2000ms and 6000ms
            int random_delay_ms = (rand() % 4000) + 2000;
            usleep(random_delay_ms * 1000); 

            // CUE PUNCH ON SCREEN
            show_punch_cue.store(true);
            long long start_time = current_timestamp();
            
            int touched = 0;
            while(!touched) {
                int max_val = 0;
                int min_val = 1023;

                // Rapid burst of 50 samples from CH0 tracking highest and lowest values
                for(int i = 0; i < 50; i++) {
                    int val = read_adc(fd, 0); 
                    if(val > max_val) max_val = val;
                    if(val < min_val) min_val = val;
                }

                int wave_size = max_val - min_val;

                // If the wave size spikes, a hand is amplifying the signal, meaning it's a touch!
                if (wave_size > CALIB_VAL) { // Adjust threshold based on environment
                    long long reaction_time = current_timestamp() - start_time;
                    last_reaction_time.store(reaction_time);
                    total_reaction_time += reaction_time;
                    current_wave_size.store(wave_size);
                    touched = 1; 
                }
            }

            // Hide the cue and give a 2 second breather
            show_punch_cue.store(false);
            sleep(2); 
        }

        // Drill Finished
        avg_reaction_time.store(total_reaction_time / 5);
        drill_completed.store(true);
        start_reflex_drill.store(false); // Reset to idle state
    }

    close(fd);
}

