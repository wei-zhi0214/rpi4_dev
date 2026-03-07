#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif


static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xf5dcad27, "i2c_register_driver" },
	{ 0x244502e8, "device_destroy" },
	{ 0x93b466ae, "class_destroy" },
	{ 0xb4bb9f0c, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xaa99dc30, "i2c_smbus_read_i2c_block_data" },
	{ 0xb43f9365, "ktime_get" },
	{ 0x34db050b, "_raw_spin_lock_irqsave" },
	{ 0xd35cce70, "_raw_spin_unlock_irqrestore" },
	{ 0xe2964344, "__wake_up" },
	{ 0x8da6585d, "__stack_chk_fail" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0xa6a39e1d, "i2c_del_driver" },
	{ 0x430a0a3a, "devm_kmalloc" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0x930682bf, "i2c_smbus_write_byte_data" },
	{ 0x7591f779, "devm_request_threaded_irq" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x5ef320f9, "cdev_init" },
	{ 0xf69f59fa, "cdev_add" },
	{ 0x5c0342c9, "_dev_err" },
	{ 0xbda0b248, "__class_create" },
	{ 0x7db40d38, "device_create" },
	{ 0x20cf1d7c, "_dev_info" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x1000e51, "schedule" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0x92540fbf, "finish_wait" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0x2d28d689, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cinvensense,mpu6050");
MODULE_ALIAS("of:N*T*Cinvensense,mpu6050C*");

MODULE_INFO(srcversion, "D2F4D9EF7F2CEA2995B0630");
