/*

	For test parameter in modules 
	
*/


#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

MODULE_LICENSE("Dual BSD/GPL");

MODULE_AUTHOR("jefby");

char * whoami = "jefby";
int n = 10;
module_param(n,int,S_IRUGO);
module_param(whoami,charp,S_IRUGO);

static int hello_init(void)
{
	int i;
	for(i=0;i<n;++i)
		printk(KERN_ALERT "%d hello,world i'm %s\n",i,whoami);
	return 0;
}
static void hello_exit(void)
{
	printk(KERN_ALERT "goodbye,cruel world.\n");
}

module_init(hello_init);
module_exit(hello_exit);

