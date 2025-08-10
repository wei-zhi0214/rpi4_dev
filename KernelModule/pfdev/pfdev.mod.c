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
	{ 0x92997ed8, "_printk" },
	{ 0x491c41d4, "platform_device_unregister" },
	{ 0xb5eefcfc, "platform_driver_unregister" },
	{ 0x5c743c23, "__platform_driver_register" },
	{ 0xdcb764ad, "memset" },
	{ 0xe3d6e336, "platform_device_register_full" },
	{ 0x8da6585d, "__stack_chk_fail" },
	{ 0x2d28d689, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "FEF3AB77FB44296B71717FC");
