#include <linux/init.h>
#include <linux/module.h>

static int hello_init(void)
{
	printk(KERN_INFO "hello world enter!\n");
	return 0;
}

static void hello_exit(void)
{
	printk(KERN_INFO "Good bye crue world!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_AUTHOR("jefby");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("A simple hello world module!\n");
MODULE_ALIAS("a simple module");
