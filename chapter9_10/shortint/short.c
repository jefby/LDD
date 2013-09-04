/*
 *
 *
 *
 *
 *
 *
 */

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
#include <linux/interrupt.h>//request_irq
#include <asm/signal.h>//SA_INTERRUPT,SA_SHIRQ


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

static int irq = -1;
volatile int short_irq = -1;
module_param(irq,int,0);

//select at load time whether install a shared irq
static int share = 0;
module_param(share,int,0);


MODULE_AUTHOR("jefby");
MODULE_LICENSE("Dual BSD/GPL");

unsigned long short_buffer = 0;
unsigned long volatile short_head = 0;
volatile unsigned long short_tail = 0;
DECLARE_WAIT_QUEUE_HEAD(short_queue);//静态定义并初始化一个等待队列

enum short_modes {SHORT_DEFAULT=0,SHORT_PAUSE,SHORT_STRING,SHORT_MEMORY};



static inline void short_inc_bp(volatile unsigned long *index,unsigned long delta)
{
	unsigned long new = *index + delta;
	barrier();//禁止编译器优化
	*index = new > (short_buffer + PAGE_SIZE) ? short_buffer : new;//更新index,如果超出一页范围，则指向起始位置short_buffer
}

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
			//默认情况
		case SHORT_DEFAULT:
			while(count--){
				*(ptr++) = inb(port);
				rmb();
			}//从io端口读数据
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
int short_open(struct inode *inode,struct file *filp)
{
	extern struct file_operations short_i_fops;
	if(iminor(inode)&0x80)//use short module with interrupt
		filp->f_op = &short_i_fops;
	return 0;
}
struct file_operations short_fops = {
	.owner = THIS_MODULE,
	.read = short_read,
	.write = short_write,
	.open = short_open,
	.release = short_release,
	.poll = short_poll
};

ssize_t short_i_read(struct file *filp,char *buf,size_t count,loff_t *off)
{
	int count0;
	DEFINE_WAIT(wait);//定义并初始化一个等待队列项	

	while(short_head == short_tail){
		prepare_to_wait(&short_queue,&wait,TASK_INTERRUPTIBLE);//TASK_INTERRUPTIBLE是新进程的状态,可中断睡眠
		if(short_head == short_tail)//check是否仍有必要休眠
			schedule();//调度
		finish_wait(&short_queue,&wait);//清理等待队列项wait
	}//while
	//count0是可读取数据的字节数
	count0 = short_head - short_tail;
	if(count0 < 0)//short_head指针已经翻转
		count = short_buffer+PAGE_SIZE - short_tail;
	if(count0 < count)//如果可用的数据数量小于需要读取的数目
		count = count0;
	if(copy_to_user(buf,(char*)short_tail,count))//拷贝short_tail开始的count个大小的数据
		return -EFAULT;
	short_incr_bp(&short_tail,count);//移动指针short_tail
	return count;//返回实际读取的字节数

}
//中断写
ssize_t short_i_write(struct file *filp,const char*buf,size_t count,loff_t *f_pos)
{
	int written = 0,odd = *f_pos & 1;
	unsigned long port = short_base;//输出到并口的数据锁存器
	void *address = (void*)short_base;
	if(use_mem){
		while(written < count)
			iowrite8(0xff * ((++written+odd)&1),address);//循环写0x00,0xff
	}else{
		while(written < count)
			outb(0xff * ((++written+odd)&1),port);
	}
	*f_pos+=count;
	return written;
}
struct file_operations short_i_fops{
	.open = short_open,
	.release = short_release,
	.read = short_i_read,
	.write = short_i_write,
	.owner = THIS_MODULE
};

//中断处理函数
irqreturn_t short_interrupt(int irq,void *dev_id,struct pt_regs *regs)
{
	struct timeval tv;
	int written;

	do_gettimeofday(&tv);//获取当前时间

	//写入一个16字节的记录。假定PAGE_SIZE是16的整数倍
	written = sprintf((char*)short_head,"%08u.%06u\n",(int)(tv.tv_sec%100000000),(int)(tv.tv_usec));
	BUG_ON(written != 16);//保证written==16
	short_incr_bp(&short_head,written);//递增指针short_head
	wake_up_interruptible(&short_queue);//唤醒任何读取进程
	
	return IRQ_HANDLED;
}

static int short_init(void)
{

	int result;
	short_base = base;
	short_irq = irq;

#ifdef DEBUG
	printk(KERN_INFO "short_init function begin!\n");
#endif
	//IO-Port
	if(!use_mem){
		if(!request_region(short_base,SHORT_NR_PORTS,"short")){
		
			printk(KERN_INFO "short: can't get I/O port address 0x%lx\n",short_base);
			return -ENODEV;
		}
	}else{
	//IO-memory
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
	short_head = short_tail = short_buffer;
/*
 * irq probe 
	if(short_irq < 0 && probe == 1)
		short_kernelprobe();
	if(short_irq < 0 && probe == 2)
		short_selfprobe();
*/
	if(short_irq < 0){
		switch(short_base){
			case 0x378:short_irq = 7;break;
			case 0x278:short_irq = 2;break;
			case 0x3bc:short_irq = 5;break;
		}
	}

	if(short_irq >= 0){
		//申请short_irq号中断通道，快速，并且安装中断处理函数short_interrupt
		result = request_irq(short_irq,short_interrupt,SA_INTERRUPT,"short",NULL);
		if(result){//申请中断通道失败
			printk(KERN_INFO "short: can't get assigned irq %i\n",short_irq);
			short_irq = -1;
		}else{
			outb(0x10,short_base+2);//真正启用中断--假定其为并口
		}
	}//if(short_irq >= 0)

	return 0;
}
static void short_cleanup(void)
{
#ifdef DEBUG
	printk(KERN_INFO "short_cleanup function !\n" );
#endif
	if(short_irq >= 0){
		outb(0x00,short_base+2);//禁止中断
		if(!share)
			free_irq(short_irq,NULL);//非共享中断，则释放中断通道
	}
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
