#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

#undef unix
struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = __stringify(KBUILD_MODNAME),
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
};

static const struct modversion_info ____versions[]
__attribute_used__
__attribute__((section("__versions"))) = {
	{ 0x4977bc21, "struct_module" },
	{ 0x70db9be2, "cdev_del" },
	{ 0x7da8156e, "__kmalloc" },
	{ 0x3c22e361, "cdev_init" },
	{ 0x89b301d4, "param_get_int" },
	{ 0xd8e484f0, "register_chrdev_region" },
	{ 0x6c3397fb, "malloc_sizes" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0x98bd6f46, "param_set_int" },
	{ 0xd533bec7, "__might_sleep" },
	{ 0x1b7d4074, "printk" },
	{ 0x2da418b5, "copy_to_user" },
	{ 0x625acc81, "__down_failed_interruptible" },
	{ 0x41c5848b, "cdev_add" },
	{ 0x123d3b6a, "kmem_cache_alloc" },
	{ 0x37a0cba, "kfree" },
	{ 0x60a4461c, "__up_wakeup" },
	{ 0xf2a644fb, "copy_from_user" },
	{ 0x29537c9e, "alloc_chrdev_region" },
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=";

