#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

MODULE_AUTHOR("jefby");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("A simple character driver");

#define GLOBALFIFO_SIZE 0x1000 //全局最大内存
#define MEM_CLEAR 0x1	//清0
#define GLOBALFIFO_MAJOR 0//预设的主设备号

static int globalfifo_major = GLOBALFIFO_MAJOR;

//globalfifo设备结构体
struct globalfifo_dev
{
	struct cdev cdev;//cdev 结构体
	unsigned int current_len;//fifo有效数据长度
	unsigned char mem[GLOBALFIFO_SIZE];//全局内存
	struct semaphore sem;//并发控制用的信号量
	wait_queue_head_t r_wait;//阻塞读用的等待队列头
	wait_queue_head_t w_wait;//阻塞写用的等待队列头
};

struct globalfifo_dev *devp;

int globalfifo_open(struct inode *inode,struct file *filp)
{
	//将设备结构体的指针赋值给文件私有数据指针
	filp->private_data = devp;
	return 0;
}

int globalfifo_release(struct inode *inode,struct file*filp)
{
	return 0;
}
static long globalfifo_ioctl(struct file *filp,unsigned int cmd,unsigned long arg)
{
	struct globalfifo_dev *devp = filp->private_data;
	switch(cmd){
	case MEM_CLEAR:
//		if(down_interruptible(&devp->sem))//获得信号量
//			return -ERESTARTSYS;
		memset(devp->mem,0,GLOBALFIFO_SIZE);
//		up(&devp->sem);//释放信号量 
		printk(KERN_INFO "globalfifo is set to zero!\n");
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
static ssize_t globalfifo_read(struct file *filp,char __user *buf,size_t count,loff_t *f_pos)
{
	struct globalfifo_dev * devp = filp->private_data;
	int ret = 0;
	
	DECLARE_WAITQUEUE(wait,current);//定义等待队列

//	if(p >= GLOBALFIFO_SIZE)
//		return 0;
	down(&devp->sem);//获得信号量
	add_wait_queue(&devp->r_wait,&wait);//进入读等待队列
	//等待FIFO非空
	while(devp->current_len == 0){
		if(filp->f_flags & O_NONBLOCK){//非阻塞读写
			ret = -EAGAIN;
			goto out;
		}
	
	__set_current_state(TASK_INTERRUPTIBLE);//改变进程状态为睡眠
	up(&devp->sem);//释放信号量

	schedule();//调用其他进程执行
	if(signal_pending(current)){//如果是因为信号唤醒
		ret = -ERESTARTSYS;
		goto out2;
	}
	down(&devp->sem);//获得信号量
	}

	if(count > devp->current_len)//拷贝到用户空间
		count = devp->current_len;
	if(copy_to_user(buf,(void *)(devp->mem),count)){
		ret = -EFAULT;
		goto out;
	}
	else{
		memcpy(devp->mem,devp->mem+count,devp->current_len-count);//fifo数据前移
		devp->current_len -=count;
		printk(KERN_INFO "read %d bytes from %d\n",count,devp->current_len);
		wake_up_interruptible(&devp->w_wait);//唤醒写等待队列
		ret = count;
	}
	
	out:up(&devp->sem);//释放信号量
	out2:remove_wait_queue(&devp->r_wait,&wait);//移除等待队列
	set_current_state(TASK_RUNNING);
	return ret;
}

static ssize_t globalfifo_write(struct file *filp,const char __user *buf,size_t count,loff_t *f_pos)
{
	struct globalfifo_dev *devp = filp->private_data;
	int ret = 0;
	DECLARE_WAITQUEUE(wait,current);//定义等待队列

	down(&devp->sem);//获取信号量
	add_wait_queue(&devp->w_wait,&wait);//计入写等待队列头
	while(devp->current_len == GLOBALFIFO_SIZE){
		if(filp->f_flags &O_NONBLOCK){//如果是非阻塞队列
			ret = -EAGAIN;
			goto out;
		}
	__set_current_state(TASK_INTERRUPTIBLE);//改变进程状态为睡眠
	up(&devp->sem);
	schedule();//调度其他进程执行
	if(signal_pending(current)){//如果是因为信号唤醒
		ret = -ERESTARTSYS;
		goto out2;
	}
	down(&devp->sem);//获得信号量
	}
	if(count > GLOBALFIFO_SIZE - devp->current_len)
		count = GLOBALFIFO_SIZE - devp->current_len;
	if(copy_from_user((void*)(devp->mem+devp->current_len),buf,count)){
		ret = -EFAULT;
		goto out;
	}
	else{
		devp->current_len += count;	
		printk(KERN_INFO "write %d bytes to %d \n",count,devp->current_len);
		wake_up_interruptible(&devp->r_wait);//唤醒读等待队列
		ret = count;
	}
	out:up(&devp->sem);//释放信号量
	out2:remove_wait_queue(&devp->w_wait,&wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

static loff_t globalfifo_llseek(struct file *filp,loff_t offset,int orig)
{
	loff_t ret = 0;
	switch(orig){
	case 0://begin
		if(offset < 0){
			ret = -EINVAL;
			break;
		}
		if((unsigned int)offset > GLOBALFIFO_SIZE){
			ret = -EINVAL;
			break;
		}
		filp->f_pos = (unsigned int)offset;
		ret = filp->f_pos;
		break;
	case 1://当前位置
		if((filp->f_pos+offset)>GLOBALFIFO_SIZE){
			ret = -EINVAL;
			break;
		}
		if((filp->f_pos+offset) < 0){
			ret = -EINVAL;
			break;
		}
		filp->f_pos += offset;
		ret = filp->f_pos;
		break;
	case 2://结尾
		if(offset > 0){
			ret = -EINVAL;
			break;
		}
		if((filp->f_pos+offset) < 0){
			ret = -EINVAL;
			break;
		}
		filp->f_pos += offset;
		ret = filp->f_pos;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static const struct file_operations globalfifo_fops = {
	.owner = THIS_MODULE,
	.read = globalfifo_read,
	.write = globalfifo_write,
	.llseek = globalfifo_llseek,
	.open = globalfifo_open,
	.release = globalfifo_release,
	.compat_ioctl = globalfifo_ioctl,
};

static void globalfifo_setup_cdev(struct globalfifo_dev *dev,int index)
{
	int err,devno = MKDEV(globalfifo_major,index);
	cdev_init(&dev->cdev,&globalfifo_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev,devno,1);
	if(err)
		printk(KERN_NOTICE "Error %d adding globalfifo %d\n",err,index);	
}

int globalfifo_init(void)
{
	int result;
	dev_t devno = MKDEV(globalfifo_major,0);
	
	if(globalfifo_major)
		result = register_chrdev_region(devno,1,"globalfifo");
	else{
		result = alloc_chrdev_region(&devno,0,1,"globalfifo");
		globalfifo_major = MAJOR(devno);
	}
	if(result < 0)
		return result;
	devp = (struct globalfifo_dev*)kmalloc(sizeof(struct globalfifo_dev),GFP_KERNEL);
	if(!devp){
		result = -ENOMEM;
		goto fail_malloc;
	}
	memset(devp,0,sizeof(struct globalfifo_dev));
	globalfifo_setup_cdev(devp,0);
	//init_MUTEX(devp->sem);//初始化信号量
	sema_init(&devp->sem,1);
	init_waitqueue_head(&devp->r_wait);//初始化读等待队列头
	init_waitqueue_head(&devp->w_wait);//初始化写等待队列头
	printk(KERN_ALERT "dev major is %d , usage:sudo  mknod /dev/globalfifo c major minor",globalfifo_major);

	return 0;
fail_malloc:
	unregister_chrdev_region(devno,1);
	return result;
}

void globalfifo_exit(void)
{
	cdev_del(&devp->cdev);//注销cdev
	kfree((void*)devp);//释放设备结构体内存
	unregister_chrdev_region(MKDEV(globalfifo_major,0),1);//释放设备号
}

module_init(globalfifo_init);
module_exit(globalfifo_exit);
