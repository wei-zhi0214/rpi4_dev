#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/sched.h>

#define DEVICE_NAME "irqtasklet"
#define CLASS_NAME  "irqtask"
#define GPIO_NUM    17   // GPIO17 for interrupt

static dev_t dev_num;
static struct cdev irq_cdev;
static struct class *irq_class;
static struct device *irq_device;

static int irq_number;
static DECLARE_WAIT_QUEUE_HEAD(wq);
static int flag = 0;

static struct tasklet_struct my_tasklet;

static void tasklet_func(unsigned long data)
{
    pr_info("irqtasklet: tasklet function executed\n");
    flag = 1;
    wake_up_interruptible(&wq);
}

static irqreturn_t irq_handler(int irq, void *dev_id)
{
    pr_info("irqtasklet: interrupt %d occurred, scheduling tasklet\n", irq);
    tasklet_schedule(&my_tasklet);
    return IRQ_HANDLED;
}

static int irqtasklet_open(struct inode *inode, struct file *file)
{
    pr_info("irqtasklet: device opened\n");
    return 0;
}

static int irqtasklet_release(struct inode *inode, struct file *file)
{
    pr_info("irqtasklet: device closed\n");
    return 0;
}

static ssize_t irqtasklet_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    pr_info("irqtasklet: waiting for tasklet to complete...\n");
    wait_event_interruptible(wq, flag != 0);
    flag = 0;

    char msg[] = "Interrupt handled by tasklet!\n";
    if (copy_to_user(buf, msg, sizeof(msg)))
        return -EFAULT;
    return sizeof(msg);
}

static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = irqtasklet_open,
    .release = irqtasklet_release,
    .read    = irqtasklet_read,
};

static int __init irqtasklet_init(void)
{
    int ret;

    // 1. 申請裝置號
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret) return ret;

    // 2. 初始化 cdev
    cdev_init(&irq_cdev, &fops);
    ret = cdev_add(&irq_cdev, dev_num, 1);
    if (ret) goto unregister_chrdev;

    // 3. 建立 class 和 device
    irq_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(irq_class)) goto del_cdev;

    irq_device = device_create(irq_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(irq_device)) goto destroy_class;

    // 4. 設定 GPIO
    ret = gpio_request(GPIO_NUM, "irq_gpio");
    if (ret) goto destroy_device;

    gpio_direction_input(GPIO_NUM);
    irq_number = gpio_to_irq(GPIO_NUM);

    ret = request_irq(irq_number, irq_handler, IRQF_TRIGGER_FALLING, DEVICE_NAME, NULL);
    if (ret) goto free_gpio;

    // 5. 初始化 tasklet
    tasklet_init(&my_tasklet, tasklet_func, 0);

    pr_info("irqtasklet: module loaded (irq=%d)\n", irq_number);
    return 0;

free_gpio:
    gpio_free(GPIO_NUM);
destroy_device:
    device_destroy(irq_class, dev_num);
destroy_class:
    class_destroy(irq_class);
del_cdev:
    cdev_del(&irq_cdev);
unregister_chrdev:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void __exit irqtasklet_exit(void)
{
    tasklet_kill(&my_tasklet);
    free_irq(irq_number, NULL);
    gpio_free(GPIO_NUM);
    device_destroy(irq_class, dev_num);
    class_destroy(irq_class);
    cdev_del(&irq_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("irqtasklet: module unloaded\n");
}

module_init(irqtasklet_init);
module_exit(irqtasklet_exit);

MODULE_LICENSE("GPL");
