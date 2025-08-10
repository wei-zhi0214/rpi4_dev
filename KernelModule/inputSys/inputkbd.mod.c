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
	{ 0x327e4b20, "input_allocate_device" },
	{ 0x29e633f7, "input_register_device" },
	{ 0xf8d77f25, "input_free_device" },
	{ 0xc6f46339, "init_timer_key" },
	{ 0x15ba50a6, "jiffies" },
	{ 0xc38c83b8, "mod_timer" },
	{ 0x92997ed8, "_printk" },
	{ 0xa7f7c41b, "input_event" },
	{ 0x82ee90dc, "timer_delete_sync" },
	{ 0xdd4901b1, "input_unregister_device" },
	{ 0x2d28d689, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "9C32BD364CCFD3B6AAD1537");
