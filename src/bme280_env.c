#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

// Variable naming conventions align with the BME280 Data Sheet

// Bosch's global variable used for pressure/humidity compensation
int32_t t_fine;

// Calibration variables (Bosch Data sheet)
uint16_t dig_T1; 
int16_t dig_T2, dig_T3;
uint16_t dig_P1; 
int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
uint8_t dig_H1, dig_H3; 
int16_t dig_H2, dig_H4, dig_H5; 
int8_t dig_H6;

// Helper function to read multiple bytes from I2C
void read_bytes(int fd, uint8_t reg, uint8_t *data, int length) {
    write(fd, &reg, 1); // which reg to read from
    read(fd, data, length); // reads len bytes back into data
}

// Helper function to write a single byte to I2C
void write_byte(int fd, uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value}; // Pack addr & value together
    write(fd, buf, 2); // Send both in one I2C transc
}

int main() {
    int fd;
    char *filename = "/dev/i2c-1";
    int addr = 0x76; // BME280 I2C addr

    if ((fd = open(filename, O_RDWR)) < 0) {
        perror("Failed to open I2C bus"); return 1;
    }
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        perror("Failed to connect to sensor"); return 1;
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
    dig_H4 = (calib[27] << 4) | (calib[28] & 0x0F); // hardware memory saving 
    dig_H5 = (calib[29] << 4) | (calib[28] >> 4); // ^ masks lower 4 bits for H4 and shifts upper 4 bits down for H5
    dig_H6 = calib[30];

    // --- Configure and Trigger Sensor ---
    // Must set ctrl_hum (0xF2) before ctrl_meas (0xF4) or humidity setting will be ignored
    write_byte(fd, 0xF2, 0x01); // Humidity oversampling x1
    write_byte(fd, 0xF4, 0x25); // Temperature/Pressure oversampling x1, Forced mode (takes 1 reading and sleeps)
    
    usleep(50000); // Wait 50ms for the measurement to complete (datasheet requires 10ms)

    // --- Read raw ADC data ---
    uint8_t data[8];
    read_bytes(fd, 0xF7, data, 8);

    // analog to digital converter raw output 
    int32_t adc_P = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
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
    float temp_celsius = T / 100.0;
    float temp_fahrenheit = (temp_celsius * 9.0 / 5.0) + 32.0;

    // --- Bosch Compensate Humidity Algorithm ---
    // Also taken from BME280 datasheet:
    int32_t v_x1_u32r;
    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    float humidity = (uint32_t)(v_x1_u32r >> 12) / 1024.0;

    // --- Print Results ---
    printf("\n--- BME280 Environmental Data ---\n");
    printf("Temperature : %.2f °C  (%.2f °F)\n", temp_celsius, temp_fahrenheit);
    printf("Humidity    : %.2f %%\n", humidity);
    printf("---------------------------------\n\n");

    close(fd);
    return 0;
}
