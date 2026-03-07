# GPIO  

## introduction
This module is show how to use gpio.

###  GPIO api
request a gpio, use gpio_request.
release a gpio, use gpio_free.

```c
/* request GPIO, returning 0 or negative errno.
 * non-null labels may be useful for diagnostics.
 */
int gpio_request(unsigned gpio, const char *label);

/* release previously-claimed GPIO */
void gpio_free(unsigned gpio);
```

set gpio as input / output.

```c
/* set as input or output, returning 0 or negative errno */
int gpio_direction_input(unsigned gpio);

@para:value is default output.
int gpio_direction_output(unsigned gpio, int value);
```
get / set gpio value.

```c
/* GPIO INPUT:  return zero or nonzero */
int gpio_get_value(unsigned gpio);

/* GPIO OUTPUT */
void gpio_set_value(unsigned gpio, int value);
```
ref:https://docs.kernel.org/6.1/driver-api/gpio/legacy.html?highlight=gpio_direction_output
---

### class 

register a class
```c
struct class *my_class;
 
my_class = class_create(THIS_MODULE, "example_class");
if (IS_ERR(my_class)) {
    pr_err("Failed to create class\n");
    return PTR_ERR(my_class);
}
```
associate with device
Once class have beeen created, it can register device to this class. Use device_create to implement.It connect device and class.
```c
struct device *my_device;
 
my_device = device_create(my_class, NULL, MKDEV(0, 0), NULL, "my_device");
if (IS_ERR(my_device)) {
    pr_err("Failed to create device\n");
    return PTR_ERR(my_device);
}
```

class attribute
Every class can have some attribute. Class arrtibute exposure to user through Sysfs, User can read or write this arrtibute to interact with kernel.
```c
struct class_attribute {
    struct attribute attr;
    ssize_t (*show)(struct class *class, char *buf);
    ssize_t (*store)(struct class *class, const char *buf, size_t count);
};
```

##  implementation

create class
```c
    myled_class = class_create(THIS_MODULE, "myled");
    if (IS_ERR(myled_class)) return PTR_ERR(myled_class);
```

class attribute
```c
static ssize_t ledctl_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", led_value);
}

static ssize_t ledctl_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count)
{
    if (buf[0] == '1') {
        gpio_set_value(LED_GPIO, 1);
        led_value = 1;
    } else if (buf[0] == '0') {
        gpio_set_value(LED_GPIO, 0);
        led_value = 0;
    }
    return count;
}

static DEVICE_ATTR(ledctl, 0660, ledctl_show, ledctl_store);
```

associate class and device
```c
    ret = device_create_file(myled_device, &dev_attr_ledctl);
    if (ret) return ret;
```

read / write
```c
static ssize_t led_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    char value;
    if (copy_from_user(&value, buf, 1))
        return -EFAULT;

    if (value == '1') {
        gpio_set_value(LED_GPIO, 1);
        led_value = 1;
    } else if (value == '0') {
        gpio_set_value(LED_GPIO, 0);
        led_value = 0;
    }
    return count;
}

static ssize_t led_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    char val_str[2];
    int len;

    if (*ppos > 0) return 0;

    val_str[0] = led_value ? '1' : '0';
    val_str[1] = '\n';
    len = 2;

    if (copy_to_user(buf, val_str, len)) return -EFAULT;

    *ppos += len;
    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = led_write,
    .read = led_read,
};
```

## test and result
test
```
insmod myled.ko

echo 1 > /sys/class/myled/led/brightness
echo 0 > /sys/class/myled/led/brightness

cat /sys/class/myled/led/brightness
```
