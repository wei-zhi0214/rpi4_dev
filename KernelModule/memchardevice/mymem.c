#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/device.h>  // for class, device
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define DEVICE_NAME "mymem"
#define MYMEM_MAGIC 'k'
#define MEM_CLEAR _IO(MYMEM_MAGIC, 0)
#define MEM_SET_SIZE _IOW(MYMEM_MAGIC, 1, int)
#define MEM_SIZE    1024

#define PROC_NAME "mymem_info"

MODULE_LICENSE("GPL");

static int major = 240;

struct mymem_dev {
    char data[MEM_SIZE];
    struct cdev cdev;
    struct mutex lock;
};

static struct mymem_dev *device;

static struct class *mymem_class;
static struct device *mymem_device;

static struct proc_dir_entry *proc_entry;

static int mymem_open(struct inode *inode, struct file *filp) {
    struct mymem_dev *dev = container_of(inode->i_cdev, struct mymem_dev, cdev);

    // 每個開啟的 fd 各自有一個 data buffer
    struct mymem_dev *priv_data = kmalloc(sizeof(struct mymem_dev), GFP_KERNEL);
    if (!priv_data)
        return -ENOMEM;

    memcpy(priv_data->data, dev->data, MEM_SIZE);
    mutex_init(&priv_data->lock);

    filp->private_data = priv_data;

    pr_info("mymem: device opened\n");
    return 0;
}

static int mymem_release(struct inode *inode, struct file *filp) {
    kfree(filp->private_data);
    pr_info("mymem: device closed\n");
    return 0;
}

static ssize_t mymem_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos) {
    struct mymem_dev *dev = filp->private_data;

    if (*ppos >= MEM_SIZE)
        return 0;

    if (*ppos + count > MEM_SIZE)
        count = MEM_SIZE - *ppos;

    mutex_lock(&dev->lock);
    if (copy_to_user(buf, dev->data + *ppos, count)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }
    *ppos += count;
    mutex_unlock(&dev->lock);
    return count;
}

static ssize_t mymem_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos) {
    struct mymem_dev *dev = filp->private_data;

    if (*ppos >= MEM_SIZE)
        return -ENOSPC;

    if (*ppos + count > MEM_SIZE)
        count = MEM_SIZE - *ppos;

    mutex_lock(&dev->lock);
    if (copy_from_user(dev->data + *ppos, buf, count)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }
    *ppos += count;
    mutex_unlock(&dev->lock);
    return count;
}

static long mymem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct mymem_dev *dev = file->private_data;
    int new_size;

    printk(KERN_INFO "mymem_ioctl cmd=0x%x\n", cmd);

    if (_IOC_TYPE(cmd) != MYMEM_MAGIC)
        return -EINVAL;

    mutex_lock(&dev->lock);

    switch (cmd) {
        case MEM_CLEAR:
            memset(dev->data, 0, MEM_SIZE);
            printk(KERN_INFO "mymem: data cleared\n");
            break;
        
        case MEM_SET_SIZE:
            if (copy_from_user(&new_size, (int __user *)arg, sizeof(int))) {
                mutex_unlock(&dev->lock);
                return -EFAULT;
            }
            
            if (new_size > MEM_SIZE || new_size <= 0) {
                mutex_unlock(&dev->lock);
                return -EINVAL;
            }

            memset(dev->data, 0, new_size);
            printk(KERN_INFO "mymem: data size set to %d\n", new_size);
            break;

        default:
            mutex_unlock(&dev->lock);
            return -EINVAL;
    }

    mutex_unlock(&dev->lock);
    return 0;
}



static loff_t mymem_llseek(struct file *filp, loff_t offset, int whence) {
    loff_t newpos;

    switch (whence) {
        case 0: newpos = offset; break;              // SEEK_SET
        case 1: newpos = filp->f_pos + offset; break; // SEEK_CUR
        case 2: newpos = MEM_SIZE + offset; break;    // SEEK_END
        default: return -EINVAL;
    }

    if (newpos < 0 || newpos > MEM_SIZE)
        return -EINVAL;

    filp->f_pos = newpos;
    return newpos;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = mymem_open,
    .release = mymem_release,
    .read = mymem_read,
    .write = mymem_write,
    .unlocked_ioctl = mymem_ioctl,
    .compat_ioctl = mymem_ioctl,       // 32-bit app 必須加上此函數
    .llseek = mymem_llseek,
};

static ssize_t status_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "mymem is alive\n");
}

static ssize_t status_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count)
{
    pr_info("mymem: received via sysfs: %.*s", (int)count, buf);
    return count;
}

//==============================================================================
static int proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "mymem driver is working fine.\n");
    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

static const struct proc_ops proc_fops = {
    .proc_open    = proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static DEVICE_ATTR_RW(status);  // status → 讀寫屬性

static int __init mymem_init(void) {
    int ret;
    dev_t devno = MKDEV(major, 0);

    device = kzalloc(sizeof(struct mymem_dev), GFP_KERNEL);
    if (!device)
        return -ENOMEM;

    ret = register_chrdev_region(devno, 1, DEVICE_NAME);
    if (ret < 0)
        goto fail_region;

    cdev_init(&device->cdev, &fops);
    device->cdev.owner = THIS_MODULE;

    ret = cdev_add(&device->cdev, devno, 1);
    if (ret)
        goto fail_add;

    mutex_init(&device->lock);
    pr_info("mymem: loaded, major=%d\n", major);

    mymem_class = class_create(THIS_MODULE, "mymem");
    if (IS_ERR(mymem_class)) {
        pr_err("failed to create class\n");
        ret = PTR_ERR(mymem_class);
        goto fail_add;
    }

    mymem_device = device_create(mymem_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(mymem_device)) {
        pr_err("failed to create device\n");
        class_destroy(mymem_class);
        ret = PTR_ERR(mymem_device);
        goto fail_add;
    }

    ret = device_create_file(mymem_device, &dev_attr_status);
    if (ret) {
        pr_err("failed to create sysfs file\n");
        device_destroy(mymem_class, MKDEV(major, 0));
        class_destroy(mymem_class);
        goto fail_add;
    }

    proc_entry = proc_create(PROC_NAME, 0, NULL, &proc_fops);
    if (!proc_entry) {
        pr_err("Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    return 0;
    
fail_add:
    unregister_chrdev_region(devno, 1);
fail_region:
    kfree(device);
    return ret;
}

static void __exit mymem_exit(void) {

    cdev_del(&device->cdev);
    unregister_chrdev_region(MKDEV(major, 0), 1);
    kfree(device);

    device_remove_file(mymem_device, &dev_attr_status);
    device_destroy(mymem_class, MKDEV(major, 0));
    class_destroy(mymem_class);

    proc_remove(proc_entry);

    pr_info("mymem: unloaded\n");
}

module_init(mymem_init);
module_exit(mymem_exit);

