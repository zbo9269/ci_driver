#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xc0b1e650, "module_layout" },
	{ 0x6980fe91, "param_get_int" },
	{ 0xff964b25, "param_set_int" },
	{ 0x3c2c5af5, "sprintf" },
	{ 0x4a524e0f, "create_proc_entry" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0xd8e484f0, "register_chrdev_region" },
	{ 0x2f287f0d, "copy_to_user" },
	{ 0x362ef408, "_copy_from_user" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0x364b3fff, "up" },
	{ 0xfa082128, "down_interruptible" },
	{ 0x275e08a1, "cdev_add" },
	{ 0x9ee8630d, "cdev_init" },
	{ 0x3af98f9e, "ioremap_nocache" },
	{ 0xb72397d5, "printk" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0x770ebb34, "remove_proc_entry" },
	{ 0x2238f337, "cdev_del" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "FF2C489FF914C6C1C8D54D1");
