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
	{ 0x5c3c7387, "kstrtoull" },
	{ 0x8da6585d, "__stack_chk_fail" },
	{ 0xe783e261, "sysfs_emit" },
	{ 0xb6e6d99d, "clk_disable" },
	{ 0xb077e70a, "clk_unprepare" },
	{ 0x779b34e1, "device_remove_file" },
	{ 0xb5eefcfc, "platform_driver_unregister" },
	{ 0x430a0a3a, "devm_kmalloc" },
	{ 0x57fb4b7d, "devm_pinctrl_get" },
	{ 0x4fbc8029, "pinctrl_lookup_state" },
	{ 0x39641566, "pinctrl_select_state" },
	{ 0x658d7009, "platform_get_resource" },
	{ 0xad3a5321, "devm_ioremap_resource" },
	{ 0xdbe1f941, "of_property_read_variable_u32_array" },
	{ 0xfed47dca, "devm_clk_get" },
	{ 0x7c9a7371, "clk_prepare" },
	{ 0x213695c1, "dev_err_probe" },
	{ 0x815588a6, "clk_enable" },
	{ 0x556e4390, "clk_get_rate" },
	{ 0x5944bc29, "device_create_file" },
	{ 0xf9a482f9, "msleep" },
	{ 0x20cf1d7c, "_dev_info" },
	{ 0xed166b63, "devm_pinctrl_put" },
	{ 0x2d28d689, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cmycompany,pwm-demo");
MODULE_ALIAS("of:N*T*Cmycompany,pwm-demoC*");

MODULE_INFO(srcversion, "B233DE4CE58E01636A28151");
