#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#define MYMEM_CLEAR 0x01

int main() {
    int fd;
    char write_buf[] = "Hello from user!";
    char read_buf[100] = {0};

    fd = open("/dev/mymem", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // Step 1: Write data
    if (write(fd, write_buf, strlen(write_buf)) < 0) {
        perror("write");
        close(fd);
        return 1;
    }

    // Step 2: Seek back to start
    if (lseek(fd, 0, SEEK_SET) < 0) {
        perror("lseek");
        close(fd);
        return 1;
    }

    // Step 3: Read data
    if (read(fd, read_buf, sizeof(read_buf) - 1) < 0) {
        perror("read");
        close(fd);
        return 1;
    }

    printf("Read from device: %s\n", read_buf);

    // Step 4: Clear device buffer via ioctl
    if (ioctl(fd, MYMEM_CLEAR) < 0) {
        perror("ioctl");
        close(fd);
        return 1;
    }

    // Step 5: Seek again and read to confirm clear
    lseek(fd, 0, SEEK_SET);
    memset(read_buf, 0, sizeof(read_buf));
    read(fd, read_buf, sizeof(read_buf) - 1);

    printf("Read after clear: %s\n", read_buf);

    close(fd);
    return 0;
}

