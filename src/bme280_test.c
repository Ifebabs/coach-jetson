#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

int main(){
	int file;
	char *filename = "/dev/i2c-1";
	int addr = 0x76; //I2C address found on grid
	char reg[1] = {0xD0}; // "Who Am I" (Chip ID) register
	char data[1] = {0};
	
	printf("Opening %s... \n", filename);

	// Open I2C bus
	if ((file = open(filename, O_RDWR)) < 0){
		perror("Failed to open the I2C bus");
		return 1;
	}

	// Tell the Linux kernel which device to target
	if (ioctl(file, I2C_SLAVE, addr) < 0){
		perror("Failed to connect to the sensor");
		close(file);
		return 1;
	}


	// Write register addr to read from
	if (write(file, reg, 1) != 1){
		perror("Failed to write to the I2C bus");
		close(file);
		return 1;
	}

	// Read the 1 byte of data sent back from the sensor
	if (read(file, data, 1) != 1){
		perror("Failed to read from the I2C bus");
		close(file);
		return 1;
	}


	printf("Success! BME280 Chip ID: 0x%02X\n", data[0]);

	if (data[0] == 0x60) {
		printf("Hardware verified: This is a genuine BME280 sensor.\n");
	} else if (data[0] == 0x58) {
		printf("Hardware verified: This is a BMP280 (Temperature/Pressure only, no humidity).\n");
	} else {
		printf("Warning: Expected 0x60, but got something else.\n");
	}

	close(file);
	return 0;
}
