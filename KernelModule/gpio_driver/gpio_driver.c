#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

static int mygpio_probe(struct platform_device *pdev)
{
    int gpio;
    struct device *dev = &pdev->dev;

    gpio = of_get_named_gpio(dev->of_node, "gpios", 0);
    if (gpio < 0) {
        dev_err(dev, "Failed to get GPIO from DT\n");
        return gpio;
    }

    pr_info("mygpio: Got GPIO %d from Device Tree\n", gpio);
    return 0;
}

static int mygpio_remove(struct platform_device *pdev)
{
    pr_info("mygpio: remove\n");
    return 0;
}

static const struct of_device_id mygpio_of_match[] = {
    { .compatible = "my,gpio-device", },
    { },
};
MODULE_DEVICE_TABLE(of, mygpio_of_match);

static struct platform_driver mygpio_driver = {
    .driver = {
        .name = "mygpio_driver",
        .of_match_table = mygpio_of_match,
    },
    .probe = mygpio_probe,
    .remove = mygpio_remove,
};

module_platform_driver(mygpio_driver);

MODULE_LICENSE("GPL");

