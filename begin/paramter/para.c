#include <linux/init.h>
#include <linux/module.h>

static char *name = "linux device driver";
static int num = 400;

static int para_init(void)
{
	printk(KERN_INFO "name : %s\n",name);
	printk(KERN_INFO "num : %d\n",num);
	return 0;	
}

static void para_exit(void)
{
	printk(KERN_INFO "para module exit!\n");
}

module_init(para_init);
module_exit(para_exit);
module_param(num,int,S_IRUGO);
module_param(name,charp,S_IRUGO);

MODULE_AUTHOR("jefby");
MODULE_DESCRIPTION("a simple module for testing params");
MODULE_VERSION("v0.1");
MODULE_LICENSE("Dual GPL/BSD");
