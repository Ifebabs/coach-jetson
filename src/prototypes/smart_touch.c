#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define CALIB_VAL 750 // Hard coded based off of personal calibration of touch wave in room

// Asks MCP3008 for a single reading
int read_adc(int fd, int channel) {
    uint8_t tx[] = { 1, (8 + channel) << 4, 0 };
    // 8 sets the single ended node, 
    // channel chooses which pin,
    // and << 4 shifts it into the correct bit position for the chip's protocol 
    // 0 is a dummy byte to clock out the response
    uint8_t rx[3] = {0}; // buffer for the chips response, init to 0s

    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx, // ptr to bytes to send
        .rx_buf = (unsigned long)rx, // ptr to buffer to reply
        .len = 3,                    // transfer 3 bytes total
        .speed_hz = 1000000,         // 1 MHz clock speed
        .bits_per_word = 8,
    };
    
    ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    
    // Combine the bits to get the final 0-1023 number
    return ((rx[1] & 3) << 8) + rx[2];
}

int main() {
    int fd = open("/dev/spidev0.0", O_RDWR);
    if (fd < 0) {
        perror("Error opening SPI device");
        return 1;
    }

    printf("\n--- SMART TOUCH DSP CALIBRATED ---\n");
    printf("Hover hand over the foil to see the wave size\n\n");

    while(1) {
        int max_val = 0;
        int min_val = 1023;

        // Rapid burst of 50 samples from CH0 tracking highest and lowest values
        for(int i = 0; i < 50; i++) {
            int val = read_adc(fd, 0); // Read Channel 0
            if(val > max_val) max_val = val;
            if(val < min_val) min_val = val;
            usleep(200); // Tiny microsecond delay between shots
        }

        // Calculate the peak-to-peak size of the wave
        int wave_size = max_val - min_val;

        // If the wave is bigger than our threshold, it's a touch
        if (wave_size > CALIB_VAL) { // Hard coded based off of personal calibration of touch wave in room
            printf(" 💥 TOUCH DETECTED! (Wave Size: %d)\n", wave_size);
        } else {
            printf("Waiting... (Wave Size: %d)\n", wave_size);
        }

        usleep(100000); // Wait a 10th of a second before the next burst
        // Around 10 touch checks per second
    }

    close(fd);
    return 0;
}

