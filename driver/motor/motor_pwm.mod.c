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
	{ 0x5c743c23, "__platform_driver_register" },
	{ 0x244502e8, "device_destroy" },
	{ 0x93b466ae, "class_destroy" },
	{ 0xb4bb9f0c, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x971bff2e, "gpiod_set_value_cansleep" },
	{ 0x93817e0b, "pwm_apply_state" },
	{ 0x8da6585d, "__stack_chk_fail" },
	{ 0x430a0a3a, "devm_kmalloc" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0xdbe1f941, "of_property_read_variable_u32_array" },
	{ 0x8c566190, "devm_pwm_get" },
	{ 0xe914b571, "devm_gpiod_get" },
	{ 0xa3a092ee, "devm_gpiod_get_optional" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x5ef320f9, "cdev_init" },
	{ 0xf69f59fa, "cdev_add" },
	{ 0xbda0b248, "__class_create" },
	{ 0x7db40d38, "device_create" },
	{ 0x20cf1d7c, "_dev_info" },
	{ 0x213695c1, "dev_err_probe" },
	{ 0xb5eefcfc, "platform_driver_unregister" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0xdcb764ad, "memset" },
	{ 0x2d28d689, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cbalancert,motor-pwm");
MODULE_ALIAS("of:N*T*Cbalancert,motor-pwmC*");

MODULE_INFO(srcversion, "6828810B5F113DAC99C6D2D");
