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
	{ 0x1b3e255e, "struct_module" },
	{ 0x9941ccb8, "free_pages" },
	{ 0xedc03953, "iounmap" },
	{ 0xc192d491, "unregister_chrdev" },
	{ 0x4784e424, "__get_free_pages" },
	{ 0xd49501d4, "__release_region" },
	{ 0x2e2a77c4, "register_chrdev" },
	{ 0x9eac042a, "__ioremap" },
	{ 0x9efed5af, "iomem_resource" },
	{ 0x1a1a4f09, "__request_region" },
	{ 0x865ebccd, "ioport_resource" },
	{ 0x1b7d4074, "printk" },
	{ 0x375bf494, "iowrite8" },
	{ 0xf2a644fb, "copy_from_user" },
	{ 0x37a0cba, "kfree" },
	{ 0x2da418b5, "copy_to_user" },
	{ 0x389e200f, "ioread8" },
	{ 0x7da8156e, "__kmalloc" },
	{ 0x53a21daf, "param_get_long" },
	{ 0x1992a2ba, "param_set_long" },
	{ 0x89b301d4, "param_get_int" },
	{ 0x98bd6f46, "param_set_int" },
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=";

