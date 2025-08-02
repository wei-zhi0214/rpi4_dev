#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/sched.h>

#define DEVICE_NAME "irqwait"
#define CLASS_NAME  "irqdemo"
#define IOCTL_TRIGGER_EVENT _IO('i', 1)

MODULE_LICENSE("GPL");

static int major;
static struct class *irq_class;
static struct device *irq_device;   // ✅ global device
static struct cdev irq_cdev;

static DECLARE_WAIT_QUEUE_HEAD(wq);
static int condition = 0;
static DEFINE_MUTEX(lock);

// ---------- sysfs: status ----------
static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", condition);
}

static ssize_t status_store(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
    int val;
    if (kstrtoint(buf, 10, &val) == 0 && val == 1) {
        mutex_lock(&lock);
        condition = 1;
        mutex_unlock(&lock);
        wake_up_interruptible(&wq);
        printk(KERN_INFO "irqwait: sysfs triggered event!\n");
    }
    return count;
}
static DEVICE_ATTR_RW(status);

// ---------- fops ----------
static int irq_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "irqwait: device opened\n");
    return 0;
}

static ssize_t irq_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
    printk(KERN_INFO "irqwait: read waiting for event...\n");

    wait_event_interruptible(wq, condition != 0);

    mutex_lock(&lock);
    condition = 0;
    mutex_unlock(&lock);

    printk(KERN_INFO "irqwait: read event received, returning\n");
    return 0;
}

static long irq_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    if (cmd == IOCTL_TRIGGER_EVENT) {
        mutex_lock(&lock);
        condition = 1;
        mutex_unlock(&lock);
        wake_up_interruptible(&wq);
        printk(KERN_INFO "irqwait: ioctl triggered event!\n");
        return 0;
    }
    return -EINVAL;
}

static int irq_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "irqwait: device closed\n");
    return 0;
}

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = irq_open,
    .read           = irq_read,
    .unlocked_ioctl = irq_ioctl,
    .release        = irq_release,
};

// ---------- module init ----------
static int __init irq_init(void) {
    dev_t dev;
    int ret;

    ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    if (ret < 0) return ret;

    major = MAJOR(dev);
    cdev_init(&irq_cdev, &fops);
    ret = cdev_add(&irq_cdev, dev, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev, 1);
        return ret;
    }

    irq_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(irq_class)) {
        cdev_del(&irq_cdev);
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(irq_class);
    }

    irq_device = device_create(irq_class, NULL, dev, NULL, DEVICE_NAME);
    if (IS_ERR(irq_device)) {
        class_destroy(irq_class);
        cdev_del(&irq_cdev);
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(irq_device);
    }

    ret = device_create_file(irq_device, &dev_attr_status);
    if (ret) {
        device_destroy(irq_class, dev);
        class_destroy(irq_class);
        cdev_del(&irq_cdev);
        unregister_chrdev_region(dev, 1);
        return ret;
    }

    mutex_init(&lock);

    printk(KERN_INFO "irqwait: driver loaded, major=%d\n", major);
    return 0;
}

// ---------- module exit ----------
static void __exit irq_exit(void) {
    device_remove_file(irq_device, &dev_attr_status);
    device_destroy(irq_class, MKDEV(major, 0));
    class_destroy(irq_class);
    cdev_del(&irq_cdev);
    unregister_chrdev_region(MKDEV(major, 0), 1);
    printk(KERN_INFO "irqwait: driver unloaded\n");
}

module_init(irq_init);
module_exit(irq_exit);
