#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("chatgpt");
MODULE_DESCRIPTION("Day 17 - Platform device with memory and irq resource");

static struct resource my_resources[] = {
    {
        .start = 0x3F200000,
        .end   = 0x3F2000FF,
        .flags = IORESOURCE_MEM,
    },
    {
        .start = 37,
        .end   = 37,
        .flags = IORESOURCE_IRQ,
    }
};

static struct platform_device my_device = {
    .name = "my_dyn_device",
    .id = -1,
    .num_resources = ARRAY_SIZE(my_resources),
    .resource = my_resources,
};

static int __init my_device_init(void)
{
    pr_info("my_device: registering platform device...\n");
    return platform_device_register(&my_device);
}

static void __exit my_device_exit(void)
{
    pr_info("my_device: unregistering platform device...\n");
    platform_device_unregister(&my_device);
}

module_init(my_device_init);
module_exit(my_device_exit);
