// pwm_user_app.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define PWM_BASE   0xFE20C000  // Raspberry Pi 4 PWM1 base address
#define BLOCK_SIZE 0x28        // 控制器大小

#define PWM_CTL    0x00
#define PWM_RNG1   0x10
#define PWM_DAT1   0x14

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("用法：%s <frequency in Hz> <duty%%>\n", argv[0]);
        return -1;
    }

    int freq = atoi(argv[1]);
    int duty = atoi(argv[2]);

    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("開啟 /dev/mem 失敗");
        return -1;
    }

    void *map_base = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, PWM_BASE);
    if (map_base == MAP_FAILED) {
        perror("mmap 失敗");
        close(mem_fd);
        return -1;
    }

    volatile uint32_t *pwm_ctl = (uint32_t *)((char *)map_base + PWM_CTL);
    volatile uint32_t *pwm_rng1 = (uint32_t *)((char *)map_base + PWM_RNG1);
    volatile uint32_t *pwm_dat1 = (uint32_t *)((char *)map_base + PWM_DAT1);

    uint32_t clk = 1000000; // 1 MHz
    uint32_t period = clk / freq;
    uint32_t duty_val = (period * duty) / 100;

    *pwm_ctl = 0;
    usleep(1000);
    *pwm_rng1 = period;
    usleep(1000);
    *pwm_dat1 = duty_val;
    usleep(1000);
    *pwm_ctl = 0x81; // Enable PWM1, PWM mode

    printf("PWM 設定完成：freq = %d Hz, duty = %d%%\n", freq, duty);

    munmap(map_base, BLOCK_SIZE);
    close(mem_fd);
    return 0;
}

