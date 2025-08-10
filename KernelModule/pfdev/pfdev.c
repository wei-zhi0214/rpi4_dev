#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/kernel.h>

static int pf_probe(struct platform_device *pdev)
{
    printk(KERN_INFO "pfdev: probe called for device: %s\n", pdev->name);
    return 0;
}

static int pf_remove(struct platform_device *pdev)
{
    printk(KERN_INFO "pfdev: remove called for device: %s\n", pdev->name);
    return 0;
}

static struct platform_driver my_platform_driver = {
    .probe  = pf_probe,
    .remove = pf_remove,
    .driver = {
        .name = "my_platform_device",
        .owner = THIS_MODULE,
    }
};

static struct platform_device *my_platform_device;

static int __init pfdev_init(void)
{
    int ret;

    ret = platform_driver_register(&my_platform_driver);
    printk(KERN_INFO "pfdev: driver registered\n");

    my_platform_device = platform_device_register_simple("my_platform_device", -1, NULL, 0);
    printk(KERN_INFO "pfdev: device registered\n");

    return ret;
}

static void __exit pfdev_exit(void)
{
    platform_device_unregister(my_platform_device);
    printk(KERN_INFO "pfdev: device unregistered\n");

    platform_driver_unregister(&my_platform_driver);
    printk(KERN_INFO "pfdev: driver unregistered\n");
}

module_init(pfdev_init);
module_exit(pfdev_exit);
MODULE_LICENSE("GPL");

