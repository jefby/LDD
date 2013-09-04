#include <linux/module.h>
#include <linux/init.h>

#include <linux/moduleparam.h>//module_param
#include <linux/fs.h>//file_operations
#include <asm/io.h> //ioremap,ioread8,inb,outb....
#include <asm/system.h>//rmb,wmb,mb,barrier
#include <linux/cdev.h>
#include <linux/types.h>
#include <asm/uaccess.h>//copy_from/to_user
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/poll.h>


#define DEBUG 
#define SHORT_NR_PORTS 8 //use 8 ports for every parport by default
//module_param(name,type,perm)
static int major = 0;//dynamic by default
module_param(major,int,0);

static int use_mem = 0;//default is I/O port
module_param(use_mem,int,0);

static unsigned long  base = 0x378;
unsigned long short_base = 0;
module_param(base,long,0);

MODULE_AUTHOR("jefby");
MODULE_LICENSE("Dual BSD/GPL");

unsigned long short_buffer = 0;
enum short_modes {SHORT_DEFAULT=0,SHORT_PAUSE,SHORT_STRING,SHORT_MEMORY};

ssize_t do_short_read(struct inode *inode,struct file * filp,char __user*buf,size_t count,loff_t *off)
{
	int retval = count,minor = iminor(inode);
	unsigned long port = short_base + (minor&0x0f);
	void *address = (void*)short_base+(minor&0x0f);
	int mode = (minor &0x70) >> 4;
	unsigned char *kbuf = kmalloc(count,GFP_KERNEL),*ptr;
	
	if(!kbuf)
		return -ENOMEM;
	ptr = kbuf;
	if(use_mem)
		mode = SHORT_MEMORY;
	switch(mode){
		case SHORT_STRING:
			insb(port,ptr,count);
			rmb();
			break;
		case SHORT_DEFAULT:
			while(count--){
				*(ptr++) = inb(port);
				rmb();
			}
			break;
		case SHORT_MEMORY:
			while(count--){
				*(ptr++) = ioread8(address);
				rmb();
			}
			break;
		case SHORT_PAUSE:
			while(count--){
				*(ptr++) = inb_p(port);
				rmb();
			}
			break;
		default:
		//no more modes defined by now
			retval = -EINVAL;
			break;
	}
	if((retval>0)&&copy_to_user(buf,kbuf,retval))
		retval = -EFAULT;
	kfree(kbuf);
	return retval;
}
ssize_t short_read(struct file * filp,char __user*buf,size_t count,loff_t *off)
{
	
	return do_short_read(filp->f_dentry->d_inode,filp,buf,count,off);
}
int short_open(struct inode *inode,struct file *filp)
{
	return 0;
}
int short_release(struct inode *inode,struct file *filp)
{
	return 0;
}

ssize_t do_short_write(struct inode *inode,struct file * filp,const char *buf,size_t count,loff_t *off)
{
	int retval = count , minor = iminor(inode);
	unsigned long port = short_base + (minor & 0x0f);
	void *address = (void*)short_base + (minor & 0x0f);
	int mode = (minor & 0x70)>>4;
	unsigned char *kbuf = kmalloc(count,GFP_KERNEL),*ptr;
	
	if(!kbuf)
		return -ENOMEM;
	if(copy_from_user(kbuf,buf,count))
		return -EFAULT;
	ptr = kbuf;
	if(use_mem)
		mode = SHORT_MEMORY;
	switch(mode){
	case SHORT_PAUSE:
		while(count--){
			outb_p(*(ptr++),port);
			wmb();
		}
		break;
	case SHORT_STRING:
		outsb(port,ptr,count);
		wmb();
		break;
	case SHORT_DEFAULT:
		while(count--){
			outb(*(ptr++),port);
			wmb();
		}
		break;
	case SHORT_MEMORY:
		while(count--){
			iowrite8(*(ptr++),address);
			wmb();
		}
		break;
	default:
		retval = -EINVAL;
		return retval;
	}
	kfree(kbuf);
	return retval;
}
ssize_t short_write(struct file * filp,const char *buf,size_t count,loff_t *off)
{
	return do_short_write(filp->f_dentry->d_inode,filp,buf,count,off);
}
unsigned int short_poll(struct file *filp, struct poll_table_struct *wait)
{
	return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
}
struct file_operations short_fops = {
	.owner = THIS_MODULE,
	.read = short_read,
	.write = short_write,
	.open = short_open,
	.release = short_release,
	.poll = short_poll
};
static int short_init(void)
{

	int result;
	short_base = base;
	
#ifdef DEBUG
	printk(KERN_INFO "short_init function begin!\n");
#endif
//IO-Port
	if(!use_mem){
		if(!request_region(short_base,SHORT_NR_PORTS,"short")){
		
			printk(KERN_INFO "short: can't get I/O port address 0x%lx\n",short_base);
			return -ENODEV;
		}
	}else{//IO-memory
		if(!request_mem_region(short_base,SHORT_NR_PORTS,"short")){
			printk(KERN_INFO "short: can't get I/O mem address 0x%lx\n",short_base);
			return -ENODEV;
		}
		//also,ioremap it
		short_base = (unsigned long)ioremap(short_base,SHORT_NR_PORTS);
		//maybe should check the return value
		if(!short_base){
			printk(KERN_INFO "short: ioremap error!\n");
			return -ENOMEM;
		}
		
			
	}
	
	result = register_chrdev(major,"short",&short_fops);
	if(result < 0){
		printk(KERN_INFO "short: can't get major number\n");
		if(!use_mem)
			release_region(short_base,SHORT_NR_PORTS);
		else
			release_mem_region(short_base,SHORT_NR_PORTS);
		return result;
	}
	if(major == 0)
		 major = result;//dynamic
	short_buffer = __get_free_pages(GFP_KERNEL,0);//Never fails
	return 0;
}
static void short_cleanup(void)
{
#ifdef DEBUG
	printk(KERN_INFO "short_cleanup function !\n" );
#endif
	unregister_chrdev(major,"short");
	if(!use_mem){
		iounmap((void __iomem*)short_base);
		release_region(short_base,SHORT_NR_PORTS);
	}else{	
		release_mem_region(short_base,SHORT_NR_PORTS);
	}
	if(short_buffer) free_page(short_buffer);
}

module_init(short_init);
module_exit(short_cleanup);
