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
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x5ef320f9, "cdev_init" },
	{ 0xf69f59fa, "cdev_add" },
	{ 0xbda0b248, "__class_create" },
	{ 0x7db40d38, "device_create" },
	{ 0x20cf1d7c, "_dev_info" },
	{ 0xa6a39e1d, "i2c_del_driver" },
	{ 0x4de27cdd, "i2c_smbus_read_byte_data" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0x8da6585d, "__stack_chk_fail" },
	{ 0x2d28d689, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cinvensense,mpu6050");
MODULE_ALIAS("of:N*T*Cinvensense,mpu6050C*");
MODULE_ALIAS("i2c:mpu6050");

MODULE_INFO(srcversion, "078BBF2814CA07F9F4E644D");
