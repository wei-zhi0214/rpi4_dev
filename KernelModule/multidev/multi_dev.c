#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

static int multi_dev_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    u32 reg_val = 0;
    int irq;

    dev_info(dev, "Probing device: %s\n", dev_name(dev));

    // 取得 reg 屬性（模擬裝置 ID）
    if (of_property_read_u32(dev->of_node, "reg", &reg_val) == 0)
        dev_info(dev, "reg = 0x%x\n", reg_val);
    else
        dev_warn(dev, "No reg property found\n");

    // 取得 interrupts 屬性
    irq = platform_get_irq(pdev, 0);
    if (irq >= 0)
        dev_info(dev, "IRQ = %d\n", irq);
    else
        dev_warn(dev, "No valid IRQ found\n");

    return 0;
}

static int multi_dev_remove(struct platform_device *pdev)
{
    dev_info(&pdev->dev, "Removing device: %s\n", dev_name(&pdev->dev));
    return 0;
}

static const struct of_device_id multi_dev_of_match[] = {
    { .compatible = "my,multi-device" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, multi_dev_of_match);

static struct platform_driver multi_dev_driver = {
    .probe  = multi_dev_probe,
    .remove = multi_dev_remove,
    .driver = {
        .name = "multi_dev_driver",
        .of_match_table = multi_dev_of_match,
    },
};

module_platform_driver(multi_dev_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("你");
MODULE_DESCRIPTION("Multi-instance DT driver with reg and irq");

