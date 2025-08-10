#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/timer.h>

static struct input_dev *my_input_dev;
static struct timer_list my_timer;

static void my_timer_func(struct timer_list *t) {
    static bool press = true;

    if (press) {
        input_report_key(my_input_dev, KEY_A, 1);  // key down
        printk(KERN_INFO "inputkbd: KEY_A down\n");
    } else {
        input_report_key(my_input_dev, KEY_A, 0);  // key up
        printk(KERN_INFO "inputkbd: KEY_A up\n");
    }

    input_sync(my_input_dev);  // 必須送 sync 才會觸發 event
    press = !press;

    mod_timer(&my_timer, jiffies + msecs_to_jiffies(2000)); // 每 2 秒觸發一次
}

static int __init inputkbd_init(void) {
    int ret;

    my_input_dev = input_allocate_device();
    if (!my_input_dev) return -ENOMEM;

    my_input_dev->name = "my_virtual_keyboard";
    my_input_dev->evbit[0] = BIT_MASK(EV_KEY);
    my_input_dev->keybit[BIT_WORD(KEY_A)] |= BIT_MASK(KEY_A);

    ret = input_register_device(my_input_dev);
    if (ret) {
        input_free_device(my_input_dev);
        return ret;
    }

    timer_setup(&my_timer, my_timer_func, 0);
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(2000));

    printk(KERN_INFO "inputkbd: module loaded\n");
    return 0;
}

static void __exit inputkbd_exit(void) {
    del_timer_sync(&my_timer);
    input_unregister_device(my_input_dev);
    printk(KERN_INFO "inputkbd: module unloaded\n");
}

module_init(inputkbd_init);
module_exit(inputkbd_exit);
MODULE_LICENSE("GPL");

