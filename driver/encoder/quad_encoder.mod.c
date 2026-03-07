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
	{ 0x244502e8, "device_destroy" },
	{ 0xb4bb9f0c, "cdev_del" },
	{ 0xffb7c514, "ida_free" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0xbda0b248, "__class_create" },
	{ 0x5c743c23, "__platform_driver_register" },
	{ 0x93b466ae, "class_destroy" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x92997ed8, "_printk" },
	{ 0x430a0a3a, "devm_kmalloc" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0xe914b571, "devm_gpiod_get" },
	{ 0xa84f10d8, "gpiod_to_irq" },
	{ 0x4aceb29a, "gpiod_get_value_cansleep" },
	{ 0x7591f779, "devm_request_threaded_irq" },
	{ 0xe7a02573, "ida_alloc_range" },
	{ 0x5ef320f9, "cdev_init" },
	{ 0xf69f59fa, "cdev_add" },
	{ 0x7db40d38, "device_create" },
	{ 0x20cf1d7c, "_dev_info" },
	{ 0xb5eefcfc, "platform_driver_unregister" },
	{ 0xa7d5f92e, "ida_destroy" },
	{ 0xb43f9365, "ktime_get" },
	{ 0x34db050b, "_raw_spin_lock_irqsave" },
	{ 0xd35cce70, "_raw_spin_unlock_irqrestore" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0x8da6585d, "__stack_chk_fail" },
	{ 0xe2964344, "__wake_up" },
	{ 0x2d28d689, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cmycompany,quad-encoder");
MODULE_ALIAS("of:N*T*Cmycompany,quad-encoderC*");

MODULE_INFO(srcversion, "65F775DE575BD294F6C4246");
