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
	{ 0x20cf1d7c, "_dev_info" },
	{ 0x4de27cdd, "i2c_smbus_read_byte_data" },
	{ 0x5c0342c9, "_dev_err" },
	{ 0xa6a39e1d, "i2c_del_driver" },
	{ 0x2d28d689, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("i2c:myi2c");
MODULE_ALIAS("of:N*T*Cmycompany,myi2c");
MODULE_ALIAS("of:N*T*Cmycompany,myi2cC*");

MODULE_INFO(srcversion, "9717A88FBC9C73EF4A5455B");
