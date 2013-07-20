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
	{ 0x15b2dc7b, "module_layout" },
	{ 0x37a0cba, "kfree" },
	{ 0xe99ab841, "cdev_del" },
	{ 0x68dfc59f, "__init_waitqueue_head" },
	{ 0x60ac4203, "cdev_add" },
	{ 0x3d3774b, "cdev_init" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0x2c9189e, "kmem_cache_alloc_trace" },
	{ 0x68c70c2d, "kmalloc_caches" },
	{ 0xd8e484f0, "register_chrdev_region" },
	{ 0x2e60bace, "memcpy" },
	{ 0x2f287f0d, "copy_to_user" },
	{ 0xe45f60d8, "__wake_up" },
	{ 0x3a013b7d, "remove_wait_queue" },
	{ 0x362ef408, "_copy_from_user" },
	{ 0x4292364c, "schedule" },
	{ 0xc4554217, "up" },
	{ 0xd7bd3af2, "add_wait_queue" },
	{ 0xdd1a2871, "down" },
	{ 0xffd5a395, "default_wake_function" },
	{ 0x289ae517, "current_task" },
	{ 0x50eedeb8, "printk" },
	{ 0xb4390f9a, "mcount" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "3D6723E5D3819F389796685");
