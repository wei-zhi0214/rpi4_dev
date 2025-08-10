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
	{ 0x779b34e1, "device_remove_file" },
	{ 0x244502e8, "device_destroy" },
	{ 0x93b466ae, "class_destroy" },
	{ 0xfe990052, "gpio_free" },
	{ 0xb5eefcfc, "platform_driver_unregister" },
	{ 0x566b4d6, "of_get_named_gpio_flags" },
	{ 0x47229b5c, "gpio_request" },
	{ 0xd2cae0a8, "gpio_to_desc" },
	{ 0xc2cc4ec, "gpiod_direction_output_raw" },
	{ 0xbda0b248, "__class_create" },
	{ 0x7db40d38, "device_create" },
	{ 0x5944bc29, "device_create_file" },
	{ 0x92997ed8, "_printk" },
	{ 0xfc23e3eb, "gpiod_set_raw_value" },
	{ 0x2d28d689, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cmy,dtled");
MODULE_ALIAS("of:N*T*Cmy,dtledC*");

MODULE_INFO(srcversion, "0342E035333A75E95185F6C");
