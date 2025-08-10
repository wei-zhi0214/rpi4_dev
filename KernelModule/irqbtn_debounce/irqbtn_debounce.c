#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/jiffies.h>

#define DEVICE_NAME "irqbtn"
#define CLASS_NAME  "irqbtn"
#define GPIO_NUM    17       // BCM GPIO17
#define DEBOUNCE_MS 100       // debounce 門檻：100ms

MODULE_LICENSE("GPL");

static int major;
static struct cdev irqbtn_cdev;
static struct class *irqbtn_class;
static struct device *irqbtn_device;
static int irq_number;

static int condition = 0;
static int irq_counter = 0;
static unsigned long last_jiffies = 0;

static DECLARE_WAIT_QUEUE_HEAD(wq);
static DEFINE_MUTEX(lock);

// ---------- interrupt handler ----------
static irqreturn_t btn_irq_handler(int irq, void *dev_id) {
    unsigned long now = jiffies;

    if (time_before(now, last_jiffies + msecs_to_jiffies(DEBOUNCE_MS)))
        return IRQ_HANDLED;  // debounce：忽略太快的觸發

    last_jiffies = now;

    mutex_lock(&lock);
    condition = 1;
    irq_counter++;
    mutex_unlock(&lock);

    wake_up_interruptible(&wq);
    pr_info("irqbtn: interrupt %d occurred, count = %d\n", irq, irq_counter);
    return IRQ_HANDLED;
}

// ---------- sysfs: /sys/class/irqbtn/irqbtn/counter ----------
static ssize_t counter_show(struct device *dev,
                            struct device_attribute *attr,
                            char *buf)
{
    return sprintf(buf, "%d\n", irq_counter);
}
static DEVICE_ATTR_RO(counter);

// ---------- file operations ----------
static int irqbtn_open(struct inode *inode, struct file *file) {
    pr_info("irqbtn: device opened\n");
    return 0;
}

static ssize_t irqbtn_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
    pr_info("irqbtn: waiting for interrupt...\n");

    wait_event_interruptible(wq, condition != 0);

    mutex_lock(&lock);
    condition = 0;
    mutex_unlock(&lock);

    pr_info("irqbtn: event received\n");
    return 0;
}

static int irqbtn_release(struct inode *inode, struct file *file) {
    pr_info("irqbtn: device closed\n");
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = irqbtn_open,
    .read = irqbtn_read,
    .release = irqbtn_release,
};

// ---------- module init ----------
static int __init irqbtn_init(void) {
    dev_t dev;
    int ret;

    ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    if (ret) return ret;

    major = MAJOR(dev);
    cdev_init(&irqbtn_cdev, &fops);
    cdev_add(&irqbtn_cdev, dev, 1);

    irqbtn_class = class_create(THIS_MODULE, CLASS_NAME);
    irqbtn_device = device_create(irqbtn_class, NULL, dev, NULL, DEVICE_NAME);

    gpio_request(GPIO_NUM, "irqbtn_gpio");
    gpio_direction_input(GPIO_NUM);
    irq_number = gpio_to_irq(GPIO_NUM);

    ret = request_irq(irq_number, btn_irq_handler,
                      IRQF_TRIGGER_FALLING, "irqbtn_irq", NULL);
    if (ret) {
        gpio_free(GPIO_NUM);
        return ret;
    }

    device_create_file(irqbtn_device, &dev_attr_counter);
    mutex_init(&lock);

    pr_info("irqbtn: loaded (major=%d, irq=%d)\n", major, irq_number);
    return 0;
}

// ---------- module exit ----------
static void __exit irqbtn_exit(void) {
    free_irq(irq_number, NULL);
    gpio_free(GPIO_NUM);

    device_remove_file(irqbtn_device, &dev_attr_counter);
    device_destroy(irqbtn_class, MKDEV(major, 0));
    class_destroy(irqbtn_class);
    cdev_del(&irqbtn_cdev);
    unregister_chrdev_region(MKDEV(major, 0), 1);
    pr_info("irqbtn: unloaded\n");
}

module_init(irqbtn_init);
module_exit(irqbtn_exit);

