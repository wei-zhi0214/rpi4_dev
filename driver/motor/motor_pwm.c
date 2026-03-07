#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/gpio/consumer.h>
#include <linux/math64.h>

#include "motor_pwm.h"

#define DRV_NAME "motor-pwm"

struct motorpwm_dev {
    struct device *dev;

    struct pwm_device *pwm_l;
    struct pwm_device *pwm_r;
    u32 period_ns;

    /* L298N: 2 direction pins per motor */
    struct gpio_desc *dir_l1;
    struct gpio_desc *dir_l2;
    struct gpio_desc *dir_r1;
    struct gpio_desc *dir_r2;

    /* optional */
    struct gpio_desc *stby;

    dev_t devt;
    struct cdev cdev;
    struct class *cls;

    struct mutex lock;
    bool enabled;
};

static void motor_set_dir_l298n(struct gpio_desc *d1, struct gpio_desc *d2, s32 val_ppm)
{
    /* coast when 0: 0/0 */
    if (val_ppm == 0) {
        gpiod_set_value_cansleep(d1, 0);
        gpiod_set_value_cansleep(d2, 0);
        return;
    }

    if (val_ppm > 0) {
        /* forward: 1/0 */
        gpiod_set_value_cansleep(d1, 1);
        gpiod_set_value_cansleep(d2, 0);
    } else {
        /* reverse: 0/1 */
        gpiod_set_value_cansleep(d1, 0);
        gpiod_set_value_cansleep(d2, 1);
    }
}

static int motor_apply_one(struct motorpwm_dev *d,
                           struct pwm_device *pwm,
                           struct gpio_desc *dir1,
                           struct gpio_desc *dir2,
                           s32 val_ppm)
{
    u32 mag;

    /* magnitude in ppm (0..1,000,000) */
    if (val_ppm >= 0) mag = (u32)val_ppm;
    else              mag = (u32)(-val_ppm);
    if (mag > 1000000) mag = 1000000;

    /* direction */
    motor_set_dir_l298n(dir1, dir2, (mag == 0) ? 0 : val_ppm);

    /* duty */
    {
        u64 duty = (u64)d->period_ns * (u64)mag;
        duty = div_u64(duty, 1000000ULL);

        struct pwm_state st;
        pwm_get_state(pwm, &st);
        st.period = d->period_ns;
        st.duty_cycle = (u32)duty;
        st.enabled = d->enabled && (mag > 0);
        return pwm_apply_state(pwm, &st);
    }
}

static long motor_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    struct motorpwm_dev *d = f->private_data;
    int ret = 0;

    mutex_lock(&d->lock);

    switch (cmd) {
    case MOTOR_IOC_ENABLE: {
        u32 en;
        if (copy_from_user(&en, (void __user *)arg, sizeof(en))) {
            ret = -EFAULT;
            break;
        }
        d->enabled = (en != 0);

        if (d->stby)
            gpiod_set_value_cansleep(d->stby, d->enabled ? 1 : 0);

        (void)motor_apply_one(d, d->pwm_l, d->dir_l1, d->dir_l2, 0);
        (void)motor_apply_one(d, d->pwm_r, d->dir_r1, d->dir_r2, 0);
        break;
    }
    case MOTOR_IOC_BRAKE:
        d->enabled = false;
        if (d->stby)
            gpiod_set_value_cansleep(d->stby, 0);
        (void)motor_apply_one(d, d->pwm_l, d->dir_l1, d->dir_l2, 0);
        (void)motor_apply_one(d, d->pwm_r, d->dir_r1, d->dir_r2, 0);
        break;

    case MOTOR_IOC_SET: {
        struct motor_cmd mc;
        if (copy_from_user(&mc, (void __user *)arg, sizeof(mc))) {
            ret = -EFAULT;
            break;
        }

        if (mc.flags & MOTOR_FLAG_BRAKE) {
            d->enabled = false;
            if (d->stby)
                gpiod_set_value_cansleep(d->stby, 0);
            (void)motor_apply_one(d, d->pwm_l, d->dir_l1, d->dir_l2, 0);
            (void)motor_apply_one(d, d->pwm_r, d->dir_r1, d->dir_r2, 0);
            break;
        }

        if (mc.flags & MOTOR_FLAG_ENABLE) {
            d->enabled = true;
            if (d->stby)
                gpiod_set_value_cansleep(d->stby, 1);
        }

        /* apply left then right (close enough sync) */
        ret = motor_apply_one(d, d->pwm_l, d->dir_l1, d->dir_l2, mc.left);
        if (!ret)
            ret = motor_apply_one(d, d->pwm_r, d->dir_r1, d->dir_r2, mc.right);
        break;
    }

    default:
        ret = -ENOTTY;
        break;
    }

    mutex_unlock(&d->lock);
    return ret;
}

static int motor_open(struct inode *ino, struct file *f)
{
    struct motorpwm_dev *d = container_of(ino->i_cdev, struct motorpwm_dev, cdev);
    f->private_data = d;
    return 0;
}

static const struct file_operations motor_fops = {
    .owner          = THIS_MODULE,
    .open           = motor_open,
    .unlocked_ioctl = motor_ioctl,
    .llseek         = no_llseek,
};

static int motor_probe(struct platform_device *pdev)
{
    struct motorpwm_dev *d;
    int ret;

    d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
    if (!d) return -ENOMEM;
    d->dev = &pdev->dev;
    mutex_init(&d->lock);
    d->enabled = false;

    if (of_property_read_u32(pdev->dev.of_node, "pwm-period-ns", &d->period_ns))
        d->period_ns = 50000; /* default 20kHz */

    d->pwm_l = devm_pwm_get(&pdev->dev, "left");
    if (IS_ERR(d->pwm_l))
        return dev_err_probe(&pdev->dev, PTR_ERR(d->pwm_l), "get pwm left\n");

    d->pwm_r = devm_pwm_get(&pdev->dev, "right");
    if (IS_ERR(d->pwm_r))
        return dev_err_probe(&pdev->dev, PTR_ERR(d->pwm_r), "get pwm right\n");

    /* L298N: 2 dir gpios per motor */
    d->dir_l1 = devm_gpiod_get(&pdev->dev, "dir-left",  GPIOD_OUT_LOW);
    if (IS_ERR(d->dir_l1))
        return dev_err_probe(&pdev->dev, PTR_ERR(d->dir_l1), "dir-left\n");

    d->dir_l2 = devm_gpiod_get(&pdev->dev, "dir-left2", GPIOD_OUT_LOW);
    if (IS_ERR(d->dir_l2))
        return dev_err_probe(&pdev->dev, PTR_ERR(d->dir_l2), "dir-left2\n");

    d->dir_r1 = devm_gpiod_get(&pdev->dev, "dir-right",  GPIOD_OUT_LOW);
    if (IS_ERR(d->dir_r1))
        return dev_err_probe(&pdev->dev, PTR_ERR(d->dir_r1), "dir-right\n");

    d->dir_r2 = devm_gpiod_get(&pdev->dev, "dir-right2", GPIOD_OUT_LOW);
    if (IS_ERR(d->dir_r2))
        return dev_err_probe(&pdev->dev, PTR_ERR(d->dir_r2), "dir-right2\n");

    /* optional standby */
    d->stby = devm_gpiod_get_optional(&pdev->dev, "stby", GPIOD_OUT_LOW);

    ret = alloc_chrdev_region(&d->devt, 0, 1, "motor0");
    if (ret) return ret;

    cdev_init(&d->cdev, &motor_fops);
    d->cdev.owner = THIS_MODULE;
    ret = cdev_add(&d->cdev, d->devt, 1);
    if (ret) goto err_chrdev;

    d->cls = class_create(THIS_MODULE, "motor");
    if (IS_ERR(d->cls)) { ret = PTR_ERR(d->cls); goto err_cdev; }

    device_create(d->cls, NULL, d->devt, NULL, "motor0");

    platform_set_drvdata(pdev, d);

    dev_info(&pdev->dev, "probe ok: period_ns=%u (L298N 2-dir mode)\n", d->period_ns);
    return 0;

err_cdev:
    cdev_del(&d->cdev);
err_chrdev:
    unregister_chrdev_region(d->devt, 1);
    return ret;
}

static int motor_remove(struct platform_device *pdev)
{
    struct motorpwm_dev *d = platform_get_drvdata(pdev);

    device_destroy(d->cls, d->devt);
    class_destroy(d->cls);
    cdev_del(&d->cdev);
    unregister_chrdev_region(d->devt, 1);
    return 0;
}

static const struct of_device_id motor_of_match[] = {
    { .compatible = "balancert,motor-pwm" },
    { }
};
MODULE_DEVICE_TABLE(of, motor_of_match);

static struct platform_driver motor_driver = {
    .probe  = motor_probe,
    .remove = motor_remove,
    .driver = {
        .name = DRV_NAME,
        .of_match_table = motor_of_match,
    },
};

module_platform_driver(motor_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dual motor PWM char driver (L298N 2-dir per motor)");

