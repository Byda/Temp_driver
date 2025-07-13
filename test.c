#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>      // open()
#include <unistd.h>     // read(), write(), close()
#include <sys/ioctl.h>  // ioctl()

#define DEVICE_PATH "/dev/virt_temp"
#define IOCTL_SET_SAMPLING_RATE _IOW('t', 1, int)

void read_temperature(int fd) {
    char buf[128] = {0};
    ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
    if (bytes < 0) {
        perror("read");
    } else {
        printf("Read from device: %s", buf);
    }
}

void write_temperature(int fd, const char *temp) {
    if (write(fd, temp, strlen(temp)) < 0) {
        perror("write");
    } else {
        printf("Wrote to device: %s\n", temp);
    }
}

void set_sampling_rate(int fd, int rate) {
    if (ioctl(fd, IOCTL_SET_SAMPLING_RATE, &rate) < 0) {
        perror("ioctl");
    } else {
        printf("Sampling rate set to %d seconds\n", rate);
    }
}

int main() {
    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    printf("Device opened successfully: %s\n", DEVICE_PATH);

    // Read current temperature
    read_temperature(fd);

    // Write a new temperature (simulate sensor data)
    write_temperature(fd, "30.2\n");

    // Read again to confirm
    read_temperature(fd);

    // Change the sampling rate
    set_sampling_rate(fd, 5);

    close(fd);
    return EXIT_SUCCESS;
}
