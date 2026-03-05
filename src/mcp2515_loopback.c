// Pretty much a self test
// If the data written into the TX buffer shows up correctly in the RX buffer,
// the SPI wiring and chip logic are both functioning correctly without the need 
// for any other CAN device on the bus

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

// MCP2515 SPI Instructions
#define INSTRUCTION_WRITE 0x02
#define INSTRUCTION_READ  0x03
#define INSTRUCTION_RESET 0xC0

// MCP2515 Register Addresses
#define CANCTRL   0x0F
#define CANSTAT   0x0E
#define TXB0CTRL  0x30
#define TXB0SIDH  0x31 // Transmit Buffer 0 Standard ID High
#define TXB0DLC   0x35 // Transmit Buffer 0 Data Length Code
#define TXB0D0    0x36 // Transmit Buffer 0 Data Byte 0
#define RXB0CTRL  0x60
#define RXB0SIDH  0x61 // Receive Buffer 0 Standard ID High
#define RXB0DLC   0x65 // Receive Buffer 0 Data Length Code
#define RXB0D0    0x66 // Receive Buffer 0 Data Byte 0

// Function to reset the MCP2515
void mcp2515_reset(int fd) {
    uint8_t tx[] = { INSTRUCTION_RESET };
    struct spi_ioc_transfer tr = { 
        .tx_buf = (unsigned long)tx, 
        .len = 1, 
        .speed_hz = 1000000, 
        .bits_per_word = 8 
    };
    ioctl(fd, SPI_IOC_MESSAGE(1), &tr); // Execute SPI transfer
    usleep(10000); // Wait for reset to complete
}

// Function to write to a specific register
void mcp2515_write_register(int fd, uint8_t address, uint8_t value) {
    uint8_t tx[] = { INSTRUCTION_WRITE, address, value };
    struct spi_ioc_transfer tr = { 
        .tx_buf = (unsigned long)tx, 
        .len = 3, // Sends: [WRITE opcode, register addr, value]
        .speed_hz = 1000000, 
        .bits_per_word = 8 
    };
    ioctl(fd, SPI_IOC_MESSAGE(1), &tr); // Execute write
}

// Function to read from a specific register
uint8_t mcp2515_read_register(int fd, uint8_t address) {
    uint8_t tx[] = { INSTRUCTION_READ, address, 0x00 }; // 0x00 - placeholder
    uint8_t rx[3] = {0};
    struct spi_ioc_transfer tr = { 
        .tx_buf = (unsigned long)tx, 
        .rx_buf = (unsigned long)rx, 
        .len = 3, 
        .speed_hz = 1000000, 
        .bits_per_word = 8 };
    ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    return rx[2]; // The data (chip's response) comes back on the 3rd byte, dummy byte is clocked out
}

int main() {
    // Open SPI Bus 0, Device 1 (CE1 on Pin 26)
    int fd = open("/dev/spidev0.1", O_RDWR);
    if (fd < 0) {
        perror("Error opening SPI device. Did you connect to CE1?");
        return 1;
    }

    printf("\n--- CAN BUS LOOPBACK INITIALIZATION ---\n");

    // Reset the chip
    mcp2515_reset(fd);
    printf("1. Chip Reset Sent\n");

    // Put the chip in Loopback Mode (CANCTRL Register: 0x40)
    mcp2515_write_register(fd, CANCTRL, 0x40);
    
    // Verify changed modes
    uint8_t status = mcp2515_read_register(fd, CANSTAT);
    if ((status & 0xE0) == 0x40) {
        printf("2. SUCCESS: Chip is in Loopback Mode (CANSTAT: 0x%02X)\n", status);
    } else {
        printf("2. FAILED: Mode mismatch (CANSTAT: 0x%02X)\n", status);
        close(fd); 
        return 1;
    }

    printf("\n--- TRANSMITTING FRAME ---\n");
    // Load the Transmit Buffer (TXB0)
    mcp2515_write_register(fd, TXB0SIDH, 0x55); // Arbitrary CAN ID
    mcp2515_write_register(fd, TXB0DLC,  0x02); // Data Length: 2 bytes
    mcp2515_write_register(fd, TXB0D0,   0xAA); // Byte 0: Punch Impact Data (e.g., 170)
    mcp2515_write_register(fd, TXB0D0 + 1, 0xBB); // Byte 1: Reaction Time Data (e.g., 187)

    // Fire the transmit command (Set TXREQ bit in TXB0CTRL)
    mcp2515_write_register(fd, TXB0CTRL, 0x08);
    printf("Message loaded and TX requested!\n");
    usleep(50000); // Give fraction of a second to route internally

    printf("\n--- POLLING RECEIVE BUFFER ---\n");
    // Poll Receive Buffer (RXB0)
    uint8_t rx_id = mcp2515_read_register(fd, RXB0SIDH);
    uint8_t rx_dlc = mcp2515_read_register(fd, RXB0DLC);
    uint8_t rx_data0 = mcp2515_read_register(fd, RXB0D0);
    uint8_t rx_data1 = mcp2515_read_register(fd, RXB0D0 + 1);

    printf("Received Frame -> ID: 0x%02X | Length: %d | Data: 0x%02X 0x%02X\n", rx_id, rx_dlc, rx_data0, rx_data1);

    if (rx_data0 == 0xAA && rx_data1 == 0xBB) {
        printf("\n ✓ LOOPBACK TEST PASSED! SPI and CAN Logic are working perfectly.\n\n");
    } else {
        printf("\n ✘ LOOPBACK FAILED. Data mismatch.\n\n");
    }

    close(fd);
    return 0;
}
