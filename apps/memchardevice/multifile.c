#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#define DEVICE_PATH "/dev/mymem"
#define MYMEM_CLEAR 0x1

int main() {
    int fd1, fd2;
    char buf1[100] = {0};
    char buf2[100] = {0};

    // 1. 同時開兩個裝置 fd
    fd1 = open(DEVICE_PATH, O_RDWR);
    if (fd1 < 0) {
        perror("open fd1");
        return 1;
    }

    fd2 = open(DEVICE_PATH, O_RDWR);
    if (fd2 < 0) {
        perror("open fd2");
        close(fd1);
        return 1;
    }

    // 2. 寫入不同資料
    write(fd1, "Data from fd1", strlen("Data from fd1"));
    write(fd2, "Data from fd2", strlen("Data from fd2"));

    // 3. seek 回開頭
    lseek(fd1, 0, SEEK_SET);
    lseek(fd2, 0, SEEK_SET);

    // 4. 分別讀出來
    read(fd1, buf1, sizeof(buf1) - 1);
    read(fd2, buf2, sizeof(buf2) - 1);

    printf("Read from fd1: %s\n", buf1);
    printf("Read from fd2: %s\n", buf2);

    // 5. ioctl 清除 fd1 的 buffer
    if (ioctl(fd1, MYMEM_CLEAR) < 0) {
        perror("ioctl MYMEM_CLEAR");
    }

    // 6. seek 並再度讀取
    lseek(fd1, 0, SEEK_SET);
    memset(buf1, 0, sizeof(buf1));
    read(fd1, buf1, sizeof(buf1) - 1);
    printf("After clear, read from fd1: %s\n", buf1);

    // 7. fd2 應該還是有資料
    lseek(fd2, 0, SEEK_SET);
    memset(buf2, 0, sizeof(buf2));
    read(fd2, buf2, sizeof(buf2) - 1);
    printf("After clear, read from fd2: %s\n", buf2);

    close(fd1);
    close(fd2);
    return 0;
}

