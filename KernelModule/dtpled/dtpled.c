#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>

static int led_gpio;
static struct class *dtled_class;
static struct device *dtled_device;

static ssize_t led_brightness_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count)
{
    int val;
    if (kstrtoint(buf, 10, &val) == 0) {
        gpio_set_value(led_gpio, val ? 1 : 0);
    }
    return count;
}

static DEVICE_ATTR_WO(led_brightness); // 對應 /sys/class/dtled/led/brightness

static int dtled_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;

    led_gpio = of_get_named_gpio(np, "gpios", 0);
    if (!gpio_is_valid(led_gpio)) {
        dev_err(&pdev->dev, "invalid GPIO\n");
        return -EINVAL;
    }

    if (gpio_request(led_gpio, "dtled-gpio")) {
        dev_err(&pdev->dev, "Failed to request GPIO %d\n", led_gpio);
        return -EBUSY;
    }

    gpio_direction_output(led_gpio, 0);

    dtled_class = class_create(THIS_MODULE, "dtled");
    dtled_device = device_create(dtled_class, NULL, 0, NULL, "led");
    device_create_file(dtled_device, &dev_attr_led_brightness);

    dev_info(&pdev->dev, "probed, gpio=%d\n", led_gpio);
    return 0;
}

static int dtled_remove(struct platform_device *pdev)
{
    device_remove_file(dtled_device, &dev_attr_led_brightness);
    device_destroy(dtled_class, 0);
    class_destroy(dtled_class);
    gpio_free(led_gpio);
    return 0;
}

static const struct of_device_id dtled_of_match[] = {
    { .compatible = "my,dto-led" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, dtled_of_match);

static struct platform_driver dtled_driver = {
    .probe = dtled_probe,
    .remove = dtled_remove,
    .driver = {
        .name = "dtled",
        .of_match_table = dtled_of_match,
    },
};

module_platform_driver(dtled_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("你自己");
MODULE_DESCRIPTION("DT-based LED driver");

