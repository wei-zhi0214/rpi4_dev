#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/device.h>

static int led_gpio;
static struct class *led_class;
static struct device *led_device;

static ssize_t brightness_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    if (buf[0] == '1')
        gpio_set_value(led_gpio, 1);
    else
        gpio_set_value(led_gpio, 0);
    return count;
}

static DEVICE_ATTR_WO(brightness);

static int dtled_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;

    led_gpio = of_get_named_gpio(np, "gpios", 0);
    if (!gpio_is_valid(led_gpio))
        return -EINVAL;

    gpio_request(led_gpio, "dtled");
    gpio_direction_output(led_gpio, 0);

    led_class = class_create(THIS_MODULE, "dtled");
    led_device = device_create(led_class, NULL, 0, NULL, "led");
    device_create_file(led_device, &dev_attr_brightness);

    pr_info("dtled: probed, gpio=%d\n", led_gpio);
    return 0;
}

static int dtled_remove(struct platform_device *pdev)
{
    device_remove_file(led_device, &dev_attr_brightness);
    device_destroy(led_class, 0);
    class_destroy(led_class);
    gpio_free(led_gpio);
    return 0;
}

static const struct of_device_id dtled_of_match[] = {
    { .compatible = "my,dtled" },
    {},
};
MODULE_DEVICE_TABLE(of, dtled_of_match);

static struct platform_driver dtled_driver = {
    .driver = {
        .name = "dtled",
        .of_match_table = dtled_of_match,
    },
    .probe = dtled_probe,
    .remove = dtled_remove,
};
module_platform_driver(dtled_driver);

MODULE_LICENSE("GPL");

