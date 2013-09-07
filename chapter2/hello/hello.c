#include <linux/module.h>	/**/
#include <linux/init.h>	/*module_init module_exit*/

MODULE_AUTHOR("jefby");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("v0.1");
MODULE_DESCRIPTION("a very simple hello module!");


static int __init hello_init(void)
{
	printk(KERN_ALERT "hello,world!I'm jefby!\n");
	return 0;
}

static void __exit hello_exit(void)
{
	printk(KERN_ALERT "Goodbye,Cruel world!\n");
	return;
}

module_init(hello_init);
module_exit(hello_exit);
