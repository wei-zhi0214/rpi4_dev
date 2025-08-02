#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>

#define LED_GPIO 21  // 根據你的實體腳位調整

static struct class *myled_class;
static struct device *myled_device;
static int major;
static int led_value = 0;

// === /dev/led 的讀寫 ===
static ssize_t led_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    char value;
    if (copy_from_user(&value, buf, 1))
        return -EFAULT;

    if (value == '1') {
        gpio_set_value(LED_GPIO, 1);
        led_value = 1;
    } else if (value == '0') {
        gpio_set_value(LED_GPIO, 0);
        led_value = 0;
    }
    return count;
}

static ssize_t led_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    char val_str[2];
    int len;

    if (*ppos > 0) return 0;

    val_str[0] = led_value ? '1' : '0';
    val_str[1] = '\n';
    len = 2;

    if (copy_to_user(buf, val_str, len)) return -EFAULT;

    *ppos += len;
    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = led_write,
    .read = led_read,
};

// === sysfs: /sys/class/myled/led/ledctl ===
static ssize_t ledctl_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", led_value);
}

static ssize_t ledctl_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count)
{
    if (buf[0] == '1') {
        gpio_set_value(LED_GPIO, 1);
        led_value = 1;
    } else if (buf[0] == '0') {
        gpio_set_value(LED_GPIO, 0);
        led_value = 0;
    }
    return count;
}

static DEVICE_ATTR(ledctl, 0660, ledctl_show, ledctl_store);

// === init / exit ===
static int __init myled_init(void) {
    int ret;

    ret = gpio_request(LED_GPIO, "LED");
    if (ret) return ret;

    gpio_direction_output(LED_GPIO, 0);

    major = register_chrdev(0, "myled", &fops);
    if (major < 0) return major;

    myled_class = class_create(THIS_MODULE, "myled");
    if (IS_ERR(myled_class)) return PTR_ERR(myled_class);

    myled_device = device_create(myled_class, NULL, MKDEV(major, 0), NULL, "led");
    if (IS_ERR(myled_device)) return PTR_ERR(myled_device);

    // ✅ 加入 sysfs 控制檔 ledctl
    ret = device_create_file(myled_device, &dev_attr_ledctl);
    if (ret) return ret;

    return 0;
}

static void __exit myled_exit(void) {
    device_remove_file(myled_device, &dev_attr_ledctl);
    device_destroy(myled_class, MKDEV(major, 0));
    class_destroy(myled_class);
    unregister_chrdev(major, "myled");
    gpio_free(LED_GPIO);
}

module_init(myled_init);
module_exit(myled_exit);
MODULE_LICENSE("GPL");

