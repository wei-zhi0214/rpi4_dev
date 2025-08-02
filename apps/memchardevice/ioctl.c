#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MYMEM_MAGIC 'k'
#define MEM_CLEAR _IO(MYMEM_MAGIC, 0)
#define MEM_SET_SIZE _IOW(MYMEM_MAGIC, 1, int)

int main(void)
{
    int fd, ret;
    int new_size = 512;
    printf("MEM_CLEAR=0x%x\n", MEM_CLEAR);
    printf("MEM_SET_SIZE=0x%x\n", MEM_SET_SIZE);
    fd = open("/dev/mymem", O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    // 測試 MEM_CLEAR 指令
    ret = ioctl(fd, MEM_CLEAR, NULL);
    if (ret < 0)
        perror("ioctl MEM_CLEAR");
    else
        printf("ioctl MEM_CLEAR success.\n");

    // 測試 MEM_SET_SIZE 指令
    ret = ioctl(fd, MEM_SET_SIZE, &new_size);
    if (ret < 0)
        perror("ioctl MEM_SET_SIZE");
    else
        printf("ioctl MEM_SET_SIZE success, new size: %d.\n", new_size);

    close(fd);
    return 0;
}

