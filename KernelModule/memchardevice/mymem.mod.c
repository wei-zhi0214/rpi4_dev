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
	{ 0x37a0cba, "kfree" },
	{ 0x73c487d3, "single_open" },
	{ 0xf4118cf2, "seq_printf" },
	{ 0x4bc63b66, "kmalloc_caches" },
	{ 0x11becf84, "kmalloc_trace" },
	{ 0x4829a47e, "memcpy" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0xb4bb9f0c, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x779b34e1, "device_remove_file" },
	{ 0x244502e8, "device_destroy" },
	{ 0x93b466ae, "class_destroy" },
	{ 0x22ee8c34, "proc_remove" },
	{ 0x3fd78f3b, "register_chrdev_region" },
	{ 0x5ef320f9, "cdev_init" },
	{ 0xf69f59fa, "cdev_add" },
	{ 0xbda0b248, "__class_create" },
	{ 0x7db40d38, "device_create" },
	{ 0x5944bc29, "device_create_file" },
	{ 0x2d23ce54, "proc_create" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0xdcb764ad, "memset" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0x8da6585d, "__stack_chk_fail" },
	{ 0xc44d743b, "seq_read" },
	{ 0x4eca9ec5, "seq_lseek" },
	{ 0x6666f5d1, "single_release" },
	{ 0x2d28d689, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "3C4DFF11B2D7433AD799226");
