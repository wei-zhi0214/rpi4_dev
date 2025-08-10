#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("/dev/irqbtn", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open /dev/irqbtn");
        return 1;
    }

    printf("[APP] Waiting for interrupt...\n");
    read(fd, NULL, 0);
    printf("[APP] Interrupt received!\n");

    close(fd);
    return 0;
}

