// imu_mpu6050.c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#include "imu_uapi.h"

#define DRV_NAME "imu-mpu6050"
#define DEV_NAME "imu0"

#define RB_ORDER 6
#define RB_SIZE  (1u << RB_ORDER)
#define RB_MASK  (RB_SIZE - 1)

struct imu_rb {
    struct imu_sample buf[RB_SIZE];
    u32 head; // write
    u32 tail; // read
};

struct imu_dev {
    struct device *dev;
    struct i2c_client *client;
    int irq;

    struct imu_rb rb;
    spinlock_t rb_lock;
    wait_queue_head_t wq;

    dev_t devt;
    struct cdev cdev;
    struct class *cls;

    struct mutex open_lock;
    bool opened;
};

static inline bool rb_empty(struct imu_rb *rb) { return rb->head == rb->tail; }
static inline bool rb_full(struct imu_rb *rb)  { return (rb->head - rb->tail) >= RB_SIZE; }

static void rb_push(struct imu_dev *d, const struct imu_sample *s)
{
    unsigned long flags;
    spin_lock_irqsave(&d->rb_lock, flags);

    if (rb_full(&d->rb)) {
        // drop oldest
        d->rb.tail++;
    }
    d->rb.buf[d->rb.head & RB_MASK] = *s;
    d->rb.head++;

    spin_unlock_irqrestore(&d->rb_lock, flags);
}

static int rb_pop(struct imu_dev *d, struct imu_sample *out)
{
    unsigned long flags;
    int ok = 0;

    spin_lock_irqsave(&d->rb_lock, flags);
    if (!rb_empty(&d->rb)) {
        *out = d->rb.buf[d->rb.tail & RB_MASK];
        d->rb.tail++;
        ok = 1;
    }
    spin_unlock_irqrestore(&d->rb_lock, flags);
    return ok;
}

static int mpu6050_init_hw(struct imu_dev *d)
{
    // Minimal init:
    // PWR_MGMT_1 (0x6B) = 0x00 (wake)
    // SMPLRT_DIV (0x19) = 0x00
    // CONFIG (0x1A) = 0x03 (DLPF ~44Hz; safe default)
    // GYRO_CONFIG (0x1B) = 0x00 (±250 dps)
    // ACCEL_CONFIG (0x1C) = 0x00 (±2g)
    // INT_ENABLE (0x38) = 0x01 (DATA_RDY_EN)
    int ret;
    ret = i2c_smbus_write_byte_data(d->client, 0x6B, 0x00);
    if (ret < 0) return ret;
    ret = i2c_smbus_write_byte_data(d->client, 0x19, 0x00);
    if (ret < 0) return ret;
    ret = i2c_smbus_write_byte_data(d->client, 0x1A, 0x03);
    if (ret < 0) return ret;
    ret = i2c_smbus_write_byte_data(d->client, 0x1B, 0x00);
    if (ret < 0) return ret;
    ret = i2c_smbus_write_byte_data(d->client, 0x1C, 0x00);
    if (ret < 0) return ret;
    ret = i2c_smbus_write_byte_data(d->client, 0x38, 0x01);
    if (ret < 0) return ret;

    return 0;
}

static irqreturn_t imu_irq_thread(int irq, void *data)
{
    struct imu_dev *d = data;
    struct imu_sample s;
    u8 b[14];
    int ret;

    // Read ACCEL_XOUT_H (0x3B) .. GYRO_ZOUT_L
    ret = i2c_smbus_read_i2c_block_data(d->client, 0x3B, 14, b);
    if (ret < 0)
        return IRQ_HANDLED;

    s.ts_ns = ktime_get_ns();

    s.ax = (s16)((b[0] << 8) | b[1]);
    s.ay = (s16)((b[2] << 8) | b[3]);
    s.az = (s16)((b[4] << 8) | b[5]);
    // b[6..7] = temp (ignored)
    s.gx = (s16)((b[8] << 8) | b[9]);
    s.gy = (s16)((b[10] << 8) | b[11]);
    s.gz = (s16)((b[12] << 8) | b[13]);

    rb_push(d, &s);
    wake_up_interruptible(&d->wq);

    return IRQ_HANDLED;
}

static int imu_open(struct inode *ino, struct file *f)
{
    struct imu_dev *d = container_of(ino->i_cdev, struct imu_dev, cdev);

    mutex_lock(&d->open_lock);
    if (d->opened) {
        mutex_unlock(&d->open_lock);
        return -EBUSY;
    }
    d->opened = true;
    mutex_unlock(&d->open_lock);

    f->private_data = d;
    return 0;
}

static int imu_release(struct inode *ino, struct file *f)
{
    struct imu_dev *d = f->private_data;
    mutex_lock(&d->open_lock);
    d->opened = false;
    mutex_unlock(&d->open_lock);
    return 0;
}

static ssize_t imu_read(struct file *f, char __user *ubuf, size_t len, loff_t *off)
{
    struct imu_dev *d = f->private_data;
    struct imu_sample s;

    if (len < sizeof(s))
        return -EINVAL;

    // Non-blocking: return -EAGAIN if empty
    if (f->f_flags & O_NONBLOCK) {
        if (!rb_pop(d, &s))
            return -EAGAIN;
    } else {
        // Blocking: wait until data available
        int ret = wait_event_interruptible(d->wq, !rb_empty(&d->rb));
        if (ret)
            return ret;
        if (!rb_pop(d, &s))
            return -EIO; // should not happen
    }

    if (copy_to_user(ubuf, &s, sizeof(s)))
        return -EFAULT;

    return sizeof(s);
}

static __poll_t imu_poll(struct file *f, poll_table *wait)
{
    struct imu_dev *d = f->private_data;
    __poll_t mask = 0;

    poll_wait(f, &d->wq, wait);
    if (!rb_empty(&d->rb))
        mask |= POLLIN | POLLRDNORM;

    return mask;
}

static const struct file_operations imu_fops = {
    .owner   = THIS_MODULE,
    .open    = imu_open,
    .release = imu_release,
    .read    = imu_read,
    .poll    = imu_poll,
    .llseek  = no_llseek,
};

static int imu_create_chrdev(struct imu_dev *d)
{
    int ret;

    ret = alloc_chrdev_region(&d->devt, 0, 1, DEV_NAME);
    if (ret) return ret;

    cdev_init(&d->cdev, &imu_fops);
    d->cdev.owner = THIS_MODULE;
    ret = cdev_add(&d->cdev, d->devt, 1);
    if (ret) goto err_chr;

    d->cls = class_create(THIS_MODULE, "imu");
    if (IS_ERR(d->cls)) { ret = PTR_ERR(d->cls); goto err_cdev; }

    device_create(d->cls, NULL, d->devt, NULL, DEV_NAME);
    return 0;

err_cdev:
    cdev_del(&d->cdev);
err_chr:
    unregister_chrdev_region(d->devt, 1);
    return ret;
}

static void imu_destroy_chrdev(struct imu_dev *d)
{
    device_destroy(d->cls, d->devt);
    class_destroy(d->cls);
    cdev_del(&d->cdev);
    unregister_chrdev_region(d->devt, 1);
}

static int imu_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
    struct imu_dev *d;
    int ret;

    d = devm_kzalloc(&client->dev, sizeof(*d), GFP_KERNEL);
    if (!d) return -ENOMEM;

    d->dev = &client->dev;
    d->client = client;
    spin_lock_init(&d->rb_lock);
    init_waitqueue_head(&d->wq);
    mutex_init(&d->open_lock);
    d->opened = false;

    i2c_set_clientdata(client, d);

    // Init MPU6050 registers
    ret = mpu6050_init_hw(d);
    if (ret) {
        dev_err(d->dev, "hw init failed: %d\n", ret);
        return ret;
    }

    // Request threaded IRQ (hard irq -> wake thread)
    d->irq = client->irq;
    if (d->irq <= 0) {
        dev_err(d->dev, "no IRQ (check DTS interrupts)\n");
        return -EINVAL;
    }

    ret = devm_request_threaded_irq(
        d->dev, d->irq,
        NULL,                // top-half
        imu_irq_thread,       // threaded handler
        IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
        DRV_NAME, d
    );
    if (ret) {
        dev_err(d->dev, "request irq failed: %d\n", ret);
        return ret;
    }

    ret = imu_create_chrdev(d);
    if (ret) {
        dev_err(d->dev, "chrdev failed: %d\n", ret);
        return ret;
    }

    dev_info(d->dev, "probe ok: /dev/%s irq=%d addr=0x%02x\n",
             DEV_NAME, d->irq, client->addr);
    return 0;
}

static void imu_remove(struct i2c_client *client)
{
    struct imu_dev *d = i2c_get_clientdata(client);
    imu_destroy_chrdev(d);
}

static const struct of_device_id imu_of_match[] = {
    { .compatible = "invensense,mpu6050" },
    { }
};
MODULE_DEVICE_TABLE(of, imu_of_match);

static struct i2c_driver imu_driver = {
    .driver = {
        .name = DRV_NAME,
        .of_match_table = imu_of_match,
    },
    .probe = imu_probe,
    .remove = imu_remove,
};

module_i2c_driver(imu_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MPU6050 IMU driver with DRDY threaded IRQ + ring buffer (/dev/imu0)");

