#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>

#define LED_GPIO 21 // RPi GPIO21 (pin 40)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("找歌機器人");
MODULE_DESCRIPTION("A simple GPIO LED driver");
MODULE_VERSION("0.1");

static int __init myled_init(void) {
    int ret;

    pr_info("myled: initializing\n");

    ret = gpio_request(LED_GPIO, "sysfs");
    if (ret) {
        pr_err("myled: failed to request GPIO %d\n", LED_GPIO);
        return ret;
    }

    gpio_direction_output(LED_GPIO, 1);  // turn LED on
    gpio_export(LED_GPIO, false);

    pr_info("myled: LED ON (GPIO %d)\n", LED_GPIO);
    return 0;
}

static void __exit myled_exit(void) {
    pr_info("myled: cleaning up\n");

    gpio_set_value(LED_GPIO, 0);  // turn LED off
    gpio_unexport(LED_GPIO);
    gpio_free(LED_GPIO);

    pr_info("myled: LED OFF and GPIO freed\n");
}

module_init(myled_init);
module_exit(myled_exit);

