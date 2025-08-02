#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define IOCTL_TRIGGER_EVENT _IO('i', 1)

int main() {
    int fd = open("/dev/irqwait", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    printf("[APP] waiting for event...\n");
    read(fd, NULL, 0);  // read 會 block
    printf("[APP] event received!\n");

    close(fd);
    return 0;
}

