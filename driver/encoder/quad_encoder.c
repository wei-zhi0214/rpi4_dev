// quad_encoder.c (multi-device safe)
// - one global class: /sys/class/enc
// - one global chrdev region
// - each DT node gets its own minor + /dev/<node-name>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/idr.h>

#include "enc_uapi.h"

#define DRV_NAME       "quad-encoder"
#define ENC_MAX_DEVS   4   // 先留餘裕：left/right + 之後可能再擴充

// ---------- Global singleton resources ----------
static dev_t g_base_devt;
static struct class *g_enc_class;
static DEFINE_IDA(enc_ida);

// ---------- Per-device ----------
struct enc_dev {
    struct device *dev;

    struct gpio_desc *ga;
    struct gpio_desc *gb;
    int irq_a, irq_b;

    spinlock_t lock;
    s32 count;
    u8  prev_state;     // 2-bit AB

    wait_queue_head_t wq;

    int id;             // minor index
    dev_t devt;
    struct cdev cdev;

    const char *node_name; // /dev name, ex: "encoder-left"
};

static inline u8 read_ab(struct enc_dev *d)
{
    int a = gpiod_get_value_cansleep(d->ga);
    int b = gpiod_get_value_cansleep(d->gb);
    return (u8)(((a & 1) << 1) | (b & 1)); // A:bit1, B:bit0
}

/*
Quadrature transition table:
Index = (prev<<2) | curr, value = delta
00->01 +1
01->11 +1
11->10 +1
10->00 +1
Reverse is -1. Invalid transitions => 0.
*/
static const s8 qdec_table[16] = {
    /* 00->00 */  0, /* 00->01 */ +1, /* 00->10 */ -1, /* 00->11 */  0,
    /* 01->00 */ -1, /* 01->01 */  0, /* 01->10 */  0, /* 01->11 */ +1,
    /* 10->00 */ +1, /* 10->01 */  0, /* 10->10 */  0, /* 10->11 */ -1,
    /* 11->00 */  0, /* 11->01 */ -1, /* 11->10 */ +1, /* 11->11 */  0,
};

static irqreturn_t enc_irq(int irq, void *data)
{
    struct enc_dev *d = data;
    unsigned long flags;
    u8 curr, prev;
    s8 delta;

    curr = read_ab(d);

    spin_lock_irqsave(&d->lock, flags);
    prev = d->prev_state;
    delta = qdec_table[(prev << 2) | curr];
    d->count += delta;
    d->prev_state = curr;
    spin_unlock_irqrestore(&d->lock, flags);

    if (delta)
        wake_up_interruptible(&d->wq);

    return IRQ_HANDLED;
}

static int enc_open(struct inode *ino, struct file *f)
{
    struct enc_dev *d = container_of(ino->i_cdev, struct enc_dev, cdev);
    f->private_data = d;
    return 0;
}

// Snapshot read: always returns current count + timestamp
static ssize_t enc_read(struct file *f, char __user *ubuf, size_t len, loff_t *off)
{
    struct enc_dev *d = f->private_data;
    struct enc_sample s;
    unsigned long flags;

    if (len < sizeof(s))
        return -EINVAL;

    s.ts_ns = ktime_get_ns();

    spin_lock_irqsave(&d->lock, flags);
    s.count = d->count;
    spin_unlock_irqrestore(&d->lock, flags);

    if (copy_to_user(ubuf, &s, sizeof(s)))
        return -EFAULT;

    return sizeof(s);
}

static __poll_t enc_poll(struct file *f, poll_table *wait)
{
    struct enc_dev *d = f->private_data;
    poll_wait(f, &d->wq, wait);

    // 我們的 read 永遠回 snapshot，因此可讀。
    // 若你未來想「只有 count 變才可讀」，再用 seq/last_count 做條件即可。
    return POLLIN | POLLRDNORM;
}

static const struct file_operations enc_fops = {
    .owner  = THIS_MODULE,
    .open   = enc_open,
    .read   = enc_read,
    .poll   = enc_poll,
    .llseek = no_llseek,
};

static int enc_chrdev_create(struct enc_dev *d)
{
    int ret;

    d->id = ida_alloc(&enc_ida, GFP_KERNEL);
    if (d->id < 0)
        return d->id;

    if (d->id >= ENC_MAX_DEVS) {
        ida_free(&enc_ida, d->id);
        return -ENOSPC;
    }

    d->devt = MKDEV(MAJOR(g_base_devt), MINOR(g_base_devt) + d->id);

    cdev_init(&d->cdev, &enc_fops);
    d->cdev.owner = THIS_MODULE;

    ret = cdev_add(&d->cdev, d->devt, 1);
    if (ret) {
        ida_free(&enc_ida, d->id);
        return ret;
    }

    device_create(g_enc_class, NULL, d->devt, NULL, "%s", d->node_name);
    return 0;
}

static void enc_chrdev_destroy(struct enc_dev *d)
{
    device_destroy(g_enc_class, d->devt);
    cdev_del(&d->cdev);
    ida_free(&enc_ida, d->id);
}

static int enc_probe(struct platform_device *pdev)
{
    struct enc_dev *d;
    int ret;

    d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;

    d->dev = &pdev->dev;
    d->node_name = dev_name(&pdev->dev);  // "encoder-left"/"encoder-right"
    spin_lock_init(&d->lock);
    init_waitqueue_head(&d->wq);

    // DTS: a-gpios / b-gpios
    d->ga = devm_gpiod_get(&pdev->dev, "a", GPIOD_IN);
    if (IS_ERR(d->ga))
        return PTR_ERR(d->ga);

    d->gb = devm_gpiod_get(&pdev->dev, "b", GPIOD_IN);
    if (IS_ERR(d->gb))
        return PTR_ERR(d->gb);

    d->irq_a = gpiod_to_irq(d->ga);
    d->irq_b = gpiod_to_irq(d->gb);
    if (d->irq_a < 0 || d->irq_b < 0)
        return -EINVAL;

    d->prev_state = read_ab(d);

    // Request both IRQs: both edges
    ret = devm_request_irq(&pdev->dev, d->irq_a, enc_irq,
                           IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                           DRV_NAME, d);
    if (ret)
        return ret;

    ret = devm_request_irq(&pdev->dev, d->irq_b, enc_irq,
                           IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                           DRV_NAME, d);
    if (ret)
        return ret;

    ret = enc_chrdev_create(d);
    if (ret)
        return ret;

    platform_set_drvdata(pdev, d);

    dev_info(&pdev->dev, "probe ok: /dev/%s (irq_a=%d irq_b=%d)\n",
             d->node_name, d->irq_a, d->irq_b);

    return 0;
}

static int enc_remove(struct platform_device *pdev)
{
    struct enc_dev *d = platform_get_drvdata(pdev);
    enc_chrdev_destroy(d);
    return 0;
}

static const struct of_device_id enc_of_match[] = {
    { .compatible = "mycompany,quad-encoder" },
    { }
};
MODULE_DEVICE_TABLE(of, enc_of_match);

static struct platform_driver enc_driver = {
    .driver = {
        .name = DRV_NAME,
        .of_match_table = enc_of_match,
    },
    .probe = enc_probe,
    .remove = enc_remove,
};

// ---------- module init/exit ----------
static int __init enc_mod_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&g_base_devt, 0, ENC_MAX_DEVS, "enc");
    if (ret)
        return ret;

    g_enc_class = class_create(THIS_MODULE, "enc");
    if (IS_ERR(g_enc_class)) {
        ret = PTR_ERR(g_enc_class);
        unregister_chrdev_region(g_base_devt, ENC_MAX_DEVS);
        return ret;
    }

    ret = platform_driver_register(&enc_driver);
    if (ret) {
        class_destroy(g_enc_class);
        unregister_chrdev_region(g_base_devt, ENC_MAX_DEVS);
        return ret;
    }

    pr_info(DRV_NAME ": loaded (major=%d)\n", MAJOR(g_base_devt));
    return 0;
}

static void __exit enc_mod_exit(void)
{
    platform_driver_unregister(&enc_driver);
    class_destroy(g_enc_class);
    unregister_chrdev_region(g_base_devt, ENC_MAX_DEVS);
    ida_destroy(&enc_ida);
    pr_info(DRV_NAME ": unloaded\n");
}

module_init(enc_mod_init);
module_exit(enc_mod_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Quadrature AB encoder driver (GPIO IRQ) multi-device safe");

