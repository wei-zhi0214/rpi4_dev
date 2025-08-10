#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("chatgpt");
MODULE_DESCRIPTION("Day 17 - Platform driver with resource parsing");

static int my_probe(struct platform_device *pdev)
{
    struct resource *mem_res, *irq_res;

    pr_info("my_driver: probe called for device: %s\n", pdev->name);

    mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);

    if (mem_res)
        pr_info("my_driver: Memory resource -> start = %#llx, end = %#llx\n",
                (unsigned long long)mem_res->start,
                (unsigned long long)mem_res->end);
    else
        pr_info("my_driver: No memory resource found.\n");

    if (irq_res)
        pr_info("my_driver: IRQ resource -> irq = %lu\n",
                (unsigned long)irq_res->start);
    else
        pr_info("my_driver: No IRQ resource found.\n");

    return 0;
}

static int my_remove(struct platform_device *pdev)
{
    pr_info("my_driver: remove called for device: %s\n", pdev->name);
    return 0;
}

static struct platform_driver my_platform_driver = {
    .probe  = my_probe,
    .remove = my_remove,
    .driver = {
        .name = "my_dyn_device",
    },
};

static int __init my_driver_init(void)
{
    return platform_driver_register(&my_platform_driver);
}

static void __exit my_driver_exit(void)
{
    platform_driver_unregister(&my_platform_driver);
}

module_init(my_driver_init);
module_exit(my_driver_exit);
