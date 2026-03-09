#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <sys/time.h> // Added for high-resolution timing
#include <time.h>     // Added for random number seeding

#define CALIB_VAL 800 // Adjust threshold based on environment (Hard coded based off of personal calibration of touch wave in room)

// Builds upon smart_touch.c file

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

// Function to get the current time in milliseconds
long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL); // Current time
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    return milliseconds;
}

int main() {
    int fd = open("/dev/spidev0.0", O_RDWR);
    if (fd < 0) {
        perror("Error opening SPI device");
        return 1;
    }

    // Seed the random number generator
    srand(time(NULL));

    printf("\n========================================\n");
    printf("🥊 COACH JETSON: REFLEX DRILL ACTIVE 🥊\n");
    printf("========================================\n\n");

    int rounds = 5; // 5-punch drill

    for(int r = 1; r <= rounds; r++) {
        printf("Round %d: Get in your stance...\n", r);

        // Calculates a random delay between 2000ms and 6000ms
        int random_delay_ms = (rand() % 4000) + 2000;
        usleep(random_delay_ms * 1000); // usleep takes microseconds

        // Cues punch
        printf("💥 PUNCH NOW! 💥\n");
        long long start_time = current_timestamp();

        // DSP Loop (Polling as fast as possible)
        int touched = 0;
        while(!touched) {
            int max_val = 0;
            int min_val = 1023;

            // Rapid burst of 50 samples from CH0 tracking highest and lowest values
            for(int i = 0; i < 50; i++) {
                int val = read_adc(fd, 0); // Read Channel 0
                if(val > max_val) max_val = val;
                if(val < min_val) min_val = val;
                // Removed internal usleep here to keep polling fast
            }

            int wave_size = max_val - min_val;

            // If the wave size spikes, a hand is amplifying the signal, meaning it's a touch!
            if (wave_size > CALIB_VAL) { // Adjust threshold based on environment
                long long end_time = current_timestamp();
                long long reaction_time = end_time - start_time;
                
                printf("✅ Hit registered! Reaction Time: %lld ms (Wave: %d)\n\n", reaction_time, wave_size);
                touched = 1; 
            }
        }

        // Give boxer a 2 second breather before the next round
        sleep(2); 
    }

    printf("Drill complete! Great work.\n");

    close(fd);
    return 0;
}

