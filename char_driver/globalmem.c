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

#define GLOBALMEM_SIZE 0x1000 //全局最大内存
#define MEM_CLEAR 0x1	//清0
#define GLOBALMEM_MAJOR 250//预设的主设备号

static int globalmem_major = GLOBALMEM_MAJOR;

//globalmem设备结构体
struct globalmem_dev
{
	struct cdev cdev;//cdev 结构体
	unsigned char mem[GLOBALMEM_SIZE];//全局内存
};

struct globalmem_dev *devp;

int globalmem_open(struct inode *inode,struct file *filp)
{
	//将设备结构体的指针赋值给文件私有数据指针
	filp->private_data = devp;
	return 0;
}

int globalmem_release(struct inode *inode,struct file*filp)
{
	return 0;
}
static long globalmem_ioctl(struct file *filp,unsigned int cmd,unsigned long arg)
{
	struct globalmem_dev *devp = filp->private_data;
	switch(cmd){
	case MEM_CLEAR:
		memset(devp->mem,0,GLOBALMEM_SIZE);
		printk(KERN_INFO "globalmem is set to zero!\n");
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
static ssize_t globalmem_read(struct file *filp,char __user *buf,size_t count,loff_t *f_pos)
{
	struct globalmem_dev * devp = filp->private_data;
	unsigned long p = *f_pos;
	int ret = 0;

	if(p >= GLOBALMEM_SIZE)
		return 0;
	if(count > GLOBALMEM_SIZE - p)//要读的字节数太大
		count = GLOBALMEM_SIZE - p;
	if(copy_to_user(buf,(void *)(devp->mem+p),count))
		ret = -EFAULT;
	else{
		*f_pos += count;
		ret = count;
		printk(KERN_INFO "read %u bytes from %lu\n",count,p);
	}
	return ret;
}

static ssize_t globalmem_write(struct file *filp,const char __user *buf,size_t count,loff_t *f_pos)
{
	struct globalmem_dev *devp = filp->private_data;
	unsigned long p = *f_pos;
	int ret = 0;

	if(p >= GLOBALMEM_SIZE)
		return 0;
	if(count > GLOBALMEM_SIZE - p)
		count = GLOBALMEM_SIZE - p;
	if(copy_from_user((void*)(devp->mem+p),buf,count))
		ret = -EFAULT;
	else{
		*f_pos += count;
		ret = count;
		printk(KERN_INFO "write %u bytes to %lu \n",count,p);
	}
	return ret;
}

static loff_t globalmem_llseek(struct file *filp,loff_t offset,int orig)
{
	loff_t ret = 0;
	switch(orig){
	case 0://begin
		if(offset < 0){
			ret = -EINVAL;
			break;
		}
		if((unsigned int)offset > GLOBALMEM_SIZE){
			ret = -EINVAL;
			break;
		}
		filp->f_pos = (unsigned int)offset;
		ret = filp->f_pos;
		break;
	case 1://当前位置
		if((filp->f_pos+offset)>GLOBALMEM_SIZE){
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

static const struct file_operations globalmem_fops = {
	.owner = THIS_MODULE,
	.read = globalmem_read,
	.write = globalmem_write,
	.llseek = globalmem_llseek,
	.open = globalmem_open,
	.release = globalmem_release,
	.compat_ioctl = globalmem_ioctl,
};

static void globalmem_setup_cdev(struct globalmem_dev *dev,int index)
{
	int err,devno = MKDEV(globalmem_major,index);
	cdev_init(&dev->cdev,&globalmem_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev,devno,1);
	if(err)
		printk(KERN_NOTICE "Error %d adding globalmem %d\n",err,index);	
}

int globalmem_init(void)
{
	int result;
	dev_t devno = MKDEV(globalmem_major,0);
	
	if(globalmem_major)
		result = register_chrdev_region(devno,1,"globalmem");
	else{
		result = alloc_chrdev_region(&devno,0,1,"globalmem");
		globalmem_major = MAJOR(devno);
	}
	if(result < 0)
		return result;
	devp = (struct globalmem_dev*)kmalloc(sizeof(struct globalmem_dev),GFP_KERNEL);
	if(!devp){
		result = -ENOMEM;
		goto fail_malloc;
	}
	memset(devp,0,sizeof(struct globalmem_dev));
	globalmem_setup_cdev(devp,0);
	return 0;
fail_malloc:
	unregister_chrdev_region(devno,1);
	return result;
}

void globalmem_exit(void)
{
	cdev_del(&devp->cdev);//注销cdev
	kfree((void*)devp);//释放设备结构体内存
	unregister_chrdev_region(MKDEV(globalmem_major,0),1);//释放设备号
}

module_init(globalmem_init);
module_exit(globalmem_exit);
