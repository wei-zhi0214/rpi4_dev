#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include "mpu6050_ioctl.h"

#define DEVICE_NAME "mpu6050"

static struct i2c_client *mpu_client;
static dev_t dev_num;
static struct cdev mpu_cdev;
static struct class *mpu_class;

static s16 read_word(struct i2c_client *client, u8 reg)
{
    s32 hi = i2c_smbus_read_byte_data(client, reg);
    s32 lo = i2c_smbus_read_byte_data(client, reg + 1);
    if (hi < 0 || lo < 0)
        return -EIO;
    return (s16)((hi << 8) | lo);
}

static long mpu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct mpu6050_data data;

    if (cmd == MPU6050_IOCTL_GET_DATA) {
        data.accel_x = read_word(mpu_client, 0x3B);
        data.accel_y = read_word(mpu_client, 0x3D);
        data.accel_z = read_word(mpu_client, 0x3F);
        data.gyro_x  = read_word(mpu_client, 0x43);
        data.gyro_y  = read_word(mpu_client, 0x45);
        data.gyro_z  = read_word(mpu_client, 0x47);

        if (copy_to_user((void __user *)arg, &data, sizeof(data)))
            return -EFAULT;
        return 0;
    }

    return -ENOTTY;
}

static int mpu_open(struct inode *inode, struct file *file) { return 0; }
static int mpu_release(struct inode *inode, struct file *file) { return 0; }

static const struct file_operations mpu_fops = {
    .owner          = THIS_MODULE,
    .open           = mpu_open,
    .release        = mpu_release,
    .unlocked_ioctl = mpu_ioctl,
    .compat_ioctl = mpu_ioctl,
};

static int mpu_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    mpu_client = client;

    alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    cdev_init(&mpu_cdev, &mpu_fops);
    cdev_add(&mpu_cdev, dev_num, 1);
    mpu_class = class_create(THIS_MODULE, DEVICE_NAME);
    device_create(mpu_class, NULL, dev_num, NULL, DEVICE_NAME);

    dev_info(&client->dev, "MPU6050 ready.\n");
    return 0;
}

static void mpu_remove(struct i2c_client *client)
{
    device_destroy(mpu_class, dev_num);
    class_destroy(mpu_class);
    cdev_del(&mpu_cdev);
    unregister_chrdev_region(dev_num, 1);
    return ;
}

static const struct i2c_device_id mpu_id[] = {
    { "mpu6050", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, mpu_id);

static const struct of_device_id mpu_dt[] = {
    { .compatible = "invensense,mpu6050" },
    { }
};
MODULE_DEVICE_TABLE(of, mpu_dt);

static struct i2c_driver mpu_driver = {
    .driver = {
        .name = "mpu6050",
        .of_match_table = mpu_dt,
    },
    .probe = mpu_probe,
    .remove = mpu_remove,
    .id_table = mpu_id,
};

module_i2c_driver(mpu_driver);
MODULE_LICENSE("GPL");

