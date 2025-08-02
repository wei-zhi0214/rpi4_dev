#include <stdio.h>
#include <stdlib.h>

#define SYSFS_PATH "/sys/class/myled/led/ledctl"

int main(int argc, char *argv[]) {
    FILE *fp;

    if (argc != 2) {
        printf("用法: %s [0|1]\n", argv[0]);
        return 1;
    }

    // 寫入亮/滅控制值
    fp = fopen(SYSFS_PATH, "w");
    if (fp == NULL) {
        perror("無法打開 sysfs");
        return 1;
    }

    fprintf(fp, "%s", argv[1]);
    fclose(fp);

    printf("LED 已設定為 %s\n", argv[1]);
    return 0;
}

