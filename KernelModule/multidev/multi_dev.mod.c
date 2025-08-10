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
	{ 0x20cf1d7c, "_dev_info" },
	{ 0xdbe1f941, "of_property_read_variable_u32_array" },
	{ 0xa402c625, "_dev_warn" },
	{ 0x8ea0d8d, "platform_get_irq" },
	{ 0x8da6585d, "__stack_chk_fail" },
	{ 0xb5eefcfc, "platform_driver_unregister" },
	{ 0x2d28d689, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cmy,multi-device");
MODULE_ALIAS("of:N*T*Cmy,multi-deviceC*");

MODULE_INFO(srcversion, "A1A99B5F8E048702CBD33E4");
