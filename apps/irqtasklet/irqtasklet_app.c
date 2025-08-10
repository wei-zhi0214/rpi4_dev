#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    char buf[64];
    int fd = open("/dev/irqtasklet", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    printf("[APP] Waiting for tasklet to finish...\n");
    int len = read(fd, buf, sizeof(buf));
    if (len > 0) {
        buf[len] = '\0';
        printf("[APP] Received from kernel: %s\n", buf);
    }

    close(fd);
    return 0;
}

