#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <poll.h> // library for hardware interrupts

// MCP2515 SPI Instructions
#define INSTRUCTION_WRITE 0x02
#define INSTRUCTION_READ  0x03
#define INSTRUCTION_RESET 0xC0

// MCP2515 Register Addresses
#define CANCTRL   0x0F
#define CANSTAT   0x0E
#define CANINTE   0x2B // Interrupt Enable Register (controlls which events trigger the pin)
#define CANINTF   0x2C // Interrupt Flag Register (tells which event just fired)
#define TXB0CTRL  0x30
#define TXB0SIDH  0x31 // Transmit Buffer 0 Standard ID High
#define TXB0DLC   0x35 // Transmit Buffer 0 Data Length Code
#define TXB0D0    0x36 // Transmit Buffer 0 Data Byte 0
#define RXB0CTRL  0x60
#define RXB0SIDH  0x61 // Receive Buffer 0 Standard ID High
#define RXB0DLC   0x65 // Receive Buffer 0 Data Length Code
#define RXB0D0    0x66 // Receive Buffer 0 Data Byte 0

#define GPIO_PIN "149" // Jetson Nano Pin 29 (Linux GPIO sysfs interface expects text files )

// Function to write to a specific register
void mcp2515_write_register(int fd, uint8_t address, uint8_t value) {
    uint8_t tx[] = { INSTRUCTION_WRITE, address, value };
    struct spi_ioc_transfer tr = { 
        .tx_buf = (unsigned long)tx, 
        .len = 3, // Sends: [WRITE opcode, register addr, value]
        .speed_hz = 1000000, 
        .bits_per_word = 8 
    };
    ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}

// Function to read from a specific register
uint8_t mcp2515_read_register(int fd, uint8_t address) {
    uint8_t tx[] = { INSTRUCTION_READ, address, 0x00 };
    uint8_t rx[3] = {0};
    struct spi_ioc_transfer tr = { 
        .tx_buf = (unsigned long)tx, 
        .rx_buf = (unsigned long)rx, 
        .len = 3, 
        .speed_hz = 1000000, 
        .bits_per_word = 8 
    };
    ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    return rx[2]; // The data (chip's response) comes back on the 3rd byte, dummy byte is clocked out
}

// GPIO Setup Function for Interrupts 
int setup_gpio_interrupt() {
    int fd;
    char buf[64];

    // Export the GPIO pin
    fd = open("/sys/class/gpio/export", O_WRONLY);
    write(fd, GPIO_PIN, 3);
    close(fd);
    usleep(100000); // Wait for Linux to create the files

    // Set as Input
    snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%s/direction", GPIO_PIN);
    fd = open(buf, O_WRONLY);
    write(fd, "in", 2);
    close(fd);

    // Set falling edge (3.3V dropping to 0V triggers the interrupt)
    snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%s/edge", GPIO_PIN);
    fd = open(buf, O_WRONLY);
    write(fd, "falling", 7);
    close(fd);

    // Open the value file to pass to the poll() function
    snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%s/value", GPIO_PIN);
    return open(buf, O_RDONLY);
}

int main() {
    // Open SPI Bus 0, Device 1 (CE1 on Pin 26)
    int spi_fd = open("/dev/spidev0.1", O_RDWR);
    if (spi_fd < 0) { 
        perror("SPI Error"); 
        return 1; 
    }

    printf("\n--- CONFIGURING HARDWARE INTERRUPTS ---\n");

    // Reset and Set to Loopback Mode
    uint8_t tx_reset[] = { INSTRUCTION_RESET };
    struct spi_ioc_transfer tr = { 
        .tx_buf = (unsigned long)tx_reset, 
        .len = 1, 
        .speed_hz = 1000000, 
        .bits_per_word = 8 
    };

    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
    usleep(10000);
    mcp2515_write_register(spi_fd, CANCTRL, 0x40);

    // Enable Interrupt on Receive Buffer 0 Full (RX0IE bit)
    mcp2515_write_register(spi_fd, CANINTE, 0x01);
    // Clear any existing interrupt flags
    mcp2515_write_register(spi_fd, CANINTF, 0x00);

    // Setup Jetson Nano GPIO Pin 29
    int gpio_fd = setup_gpio_interrupt();
    if (gpio_fd < 0) { 
        perror("GPIO Error"); 
        return 1; 
    }

    // Structure for the poll() function
    struct pollfd pfd; // Create a poll descriptor struct
    pfd.fd = gpio_fd; // Watches the GPIO value file
    pfd.events = POLLPRI; // Priority data to read (Hardware Edge)

    // Dummy read to clear the initial state
    char c; 
    read(gpio_fd, &c, 1);

    printf("Loading transmit buffer...\n");
    mcp2515_write_register(spi_fd, TXB0SIDH, 0x55);
    mcp2515_write_register(spi_fd, TXB0DLC,  0x02);
    mcp2515_write_register(spi_fd, TXB0D0,   0xAA);
    mcp2515_write_register(spi_fd, TXB0D0 + 1, 0xBB);

    printf("Firing CAN Frame! Jetson CPU going to sleep...\n\n");
    mcp2515_write_register(spi_fd, TXB0CTRL, 0x08); // Fires CAN frame

    // CPU BLOCKS HERE: Waiting for the MCP2515 to drop the INT pin
    int ret = poll(&pfd, 1, 3000); // 3000ms timeout

    if (ret > 0) {
        if (pfd.revents & POLLPRI) {
            printf("HARDWARE INTERRUPT DETECTED! Waking up CPU to read data.\n");
            
            uint8_t rx_data0 = mcp2515_read_register(spi_fd, RXB0D0);
            uint8_t rx_data1 = mcp2515_read_register(spi_fd, RXB0D0 + 1);
            
            printf("Received: 0x%02X 0x%02X\n", rx_data0, rx_data1);

            // Clear the flag so the INT pin goes back to 3.3V
            mcp2515_write_register(spi_fd, CANINTF, 0x00); 
            printf("Interrupt Flag Cleared. Test Passed!\n\n");
        }
    } else if (ret == 0) {
        printf("Timeout: No interrupt detected after 3 seconds.\n");
    }

    close(spi_fd);
    close(gpio_fd);
    return 0;
}
