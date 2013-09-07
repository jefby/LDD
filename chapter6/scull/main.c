/*
 *
 *
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/errno.h>
#include <linux/types.h>//dev_t MAJOR MINOR MKDEV
#include <linux/fs.h>//register_chrdev_region
#include <linux/cdev.h>//cdev
#include <linux/kernel.h>//container_of
#include <asm/uaccess.h>//copy_*_user

#include "scull.h"


int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;

module_param(scull_major,int,S_IRUGO);
module_param(scull_minor,int,S_IRUGO);
module_param(scull_nr_devs,int,S_IRUGO);
module_param(scull_quantum,int,S_IRUGO);
module_param(scull_qset,int,S_IRUGO);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("jefby");


struct scull_dev *scull_devices;

//清除scull_dev所指定的设备
int scull_trim(struct scull_dev *dev)
{
	struct scull_qset *dptr,*next;
	int qset = dev->qset;//dev is not-null
	int i;

	for(dptr = dev->data;dptr;dptr=next){
		if(dptr->data){
			for(i=0;i<qset;i++)
				kfree(dptr->data[i]);//释放块中的页
			kfree(dptr->data);//释放块数据
			dptr->data = NULL;
		}
		next = dptr->next;//迭代指针
		kfree(dptr);
	}
	dev->size = 0;
	dev->quantum = scull_quantum;
	dev->qset = scull_qset;
	dev->data = NULL;
	return 0;
}


//open
int scull_open(struct inode *inode,struct file *filp)
{
	struct scull_dev *dev;//device information
	dev = container_of(inode->i_cdev,struct scull_dev,cdev);//获取到设备信息
	filp->private_data = dev;//for other methods,保存设备特有指针

	if((filp->f_flags & O_ACCMODE) == O_WRONLY){//若是以只写方式打开
		if(down_interruptible(&dev->sem))//若发生中断,则返回非零值
			return -ERESTARTSYS;
		scull_trim(dev);//ignore errors
		up(&dev->sem);//v操作
	}
	return 0;//success
}

int scull_release(struct inode *inode,struct file *filp)
{
	return 0;
}

struct scull_qset*scull_follow(struct scull_dev *dev,int n)
{
	struct scull_qset *qs=dev->data;//获取块指针
	if(!qs){
		qs=dev->data=kmalloc(sizeof(struct scull_qset),GFP_KERNEL);//分配块
		if(qs==NULL)
			return NULL;
		memset(qs,0,sizeof(struct scull_qset));
	}
	while(n--){
		if(qs->next){//下一块数据
			qs->next = kmalloc(sizeof(struct scull_qset),GFP_KERNEL);//分配块结构
			if(qs->next == NULL)
				return NULL;
			memset(qs->next,0,sizeof(struct scull_qset));//初始化为0
		}
		qs = qs->next;
		continue;
	}
	return qs;
}
//read
ssize_t scull_read(struct file *filp,char __user *buf,size_t count,loff_t *f_pos)
{
	struct scull_dev *dev=filp->private_data;//获取设备特有的数据结构指针
	struct scull_qset *dptr;
	int quantum = dev->quantum,qset = dev->qset;
	int itemsize = quantum*qset;//块数据大小
	int item,s_pos,q_pos,rest;

	ssize_t retval = 0;
	if(down_interruptible(&dev->sem))//若发生中断,则返回非0值
		return -ERESTARTSYS;
	if(*f_pos>=dev->size)//若读取的初始位置大于设备大小,则跳转到out处
		goto out;
	if(*f_pos+count > dev->size)//若读取的个数大于设备可用大小,则修改读取数目
		count = dev->size - *f_pos;

	item = (long)*f_pos/itemsize;
	rest = (long)*f_pos%itemsize;
	
	s_pos = rest/quantum;
	q_pos = rest%quantum;

	//follow the list up to the right postion
	dptr = scull_follow(dev,item);

	if(dptr == NULL || !dptr->data || !dptr->data[s_pos])
		goto out;
	
	if(count > quantum - q_pos)//最多读取一页
		count = quantum - q_pos;
	if(copy_to_user(buf,dptr->data[s_pos]+q_pos,count)){//拷贝数据到用户数据区buf
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	retval = count;
	out:
		up(&dev->sem);
		return retval;
}
//write
//
ssize_t scull_write(struct file*filp,const char __user *buf,size_t count,loff_t *f_pos)
{
	
	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dptr;
	//获取必要信息
	int quantum = dev->quantum,qset=dev->qset;
	int itemsize = quantum * qset;
	int item,s_pos,q_pos,rest;
	ssize_t retval = -ENOMEM;
	//若发生中断,返回非零值
	if(down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	
	item = (long)*f_pos/itemsize;
	rest = (long)*f_pos%itemsize;

	s_pos = rest / quantum;
	q_pos = rest % quantum;

	dptr = scull_follow(dev,item);
	
	if(dptr == NULL)
		goto out;
	if(!dptr->data){//块数据是否分配过了?
		dptr->data = kmalloc(qset*sizeof(char*),GFP_KERNEL);//未分配则使用kmalloc分配
		if(!dptr->data)//分配失败
			goto out;
		memset(dptr->data,0,sizeof(char*)*qset);//初始化
	}
	if(!dptr->data[s_pos]){//该页是否分配过?
		dptr->data[s_pos] = kmalloc(quantum,GFP_KERNEL);
		if(!dptr->data[s_pos])
			goto out;
	}
	if(count > quantum - q_pos)
		count = quantum - q_pos;
	if(copy_from_user(dptr->data[s_pos]+q_pos,buf,count)){//拷贝用户数据buf到设备
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;//更新设备指针
	retval = count;
	if(dev->size < *f_pos)
		dev->size = *f_pos;
	out:
	up(&dev->sem);//释放
	return retval;
}
//llseek
loff_t scull_llseek(struct file *filp,loff_t off,int whence)
{
	struct scull_dev *dev=filp->private_data;
	loff_t newpos;
	switch(whence){
	case 0://SEEK_SET
		newpos = off;
		break;
	case 1://SEEK_CUR
		newpos = filp->f_pos+off;
		break;
	case 2://SEEK_END
		newpos = dev->size + off;
		break;
	default:
		return -EINVAL;
	}
	if(newpos < 0) return -EINVAL;
	filp->f_pos = newpos;
	return newpos;
}
//ioctl
int scull_ioctl(struct inode *inode,struct file *filp,unsigned int cmd,unsigned long arg)
{
	int err=0,tmp;
	int retval = 0;

	if(_IOC_TYPE(cmd) != SCULL_IOC_MAGIC)	return -ENOTTY;
	if(_IOC_NR(cmd) > SCULL_IOC_MAXNR)	return -ENOTTY;

	/*
	*	transfer 'Type' is user-oriented,while
	*	access_ok is kernel-oriented,so the concept of "read" and 
	*	write is reversed
	*/	
	if(_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,(void __user *)arg,_IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd)&_IOC_WRITE)
		err = !access_ok(VERIFY_READ,(void __user*)arg,_IOC_SIZE(cmd));
	if(err)
		return -EFAULT;

	switch(cmd){
	case SCULL_IOCRESET:
		scull_quantum = SCULL_QUANTUM;
		scull_qset = SCULL_QSET;
		break;
	case SCULL_IOCSQUANTUM:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		retval=__get_user(scull_quantum,(int __user*)arg);
		break;
	case SCULL_IOCSQSET:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		retval=__get_user(scull_qset,(int __user*)arg);
		break;
	case SCULL_IOCTQUANTUM:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		scull_quantum = arg;
		break;
	case SCULL_IOCGQUANTUM:
		retval=put_user(scull_quantum,(int __user*)arg);
		break;
	case SCULL_IOCQQUANTUM:
		return scull_quantum;
	case SCULL_IOCXQUANTUM:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scull_quantum;
		retval = __get_user(scull_quantum,(int __user*)arg);
		if(retval == 0)
			retval = __put_user(tmp,(int __user*)arg);
		break;
	case SCULL_IOCHQUANTUM:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scull_quantum;
		scull_quantum = arg;
		return tmp;
	case SCULL_IOCTQSET:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		scull_qset = arg;
		break;
	case SCULL_IOCGQSET:
		retval = __put_user(scull_qset,(int __user*)arg);
		break;
	case SCULL_IOCQQSET:
		return scull_qset;
	case SCULL_IOCXQSET:
		if(!capable(CAP_SYS_ADMIN)) 
			return -EPERM;
		tmp = scull_qset;
		retval = __get_user(scull_qset,(int __user*)arg);
		if(retval == 0)
			retval = put_user(tmp,(int __user*)arg);
		break;
	case SCULL_IOCHQSET:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scull_qset;
		scull_qset = arg;
		return tmp;
	default:
		return -ENOTTY;
	}
	return retval;


}
struct file_operations scull_fops={
	.owner = THIS_MODULE,
	.llseek = scull_llseek,
	.read = scull_read,
	.write = scull_write,
	.open = scull_open,
	.release = scull_release,
	.ioctl = scull_ioctl,
};
static void scull_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(scull_major,scull_minor);
	if(scull_devices){
		for(i=0;i<scull_nr_devs;i++){
			scull_trim(scull_devices+i);//释放设备所占的内存
			cdev_del(&scull_devices[i].cdev);//从系统中删除字符设备cdev
		}
		kfree(scull_devices);//释放
	}
	//clean_up module is never called if registering failed
	unregister_chrdev_region(devno,scull_nr_devs);//释放设备号
}
//
static void scull_setup_cdev(struct scull_dev *dev,int index)
{
	int err,devno=MKDEV(scull_major,scull_minor+index);

	//用于初始化cdev的成员，并建立cdev和file_operations之间的连接
	cdev_init(&dev->cdev,&scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;
	//向系统中添加cdev,完成字符设备注册
	err = cdev_add(&dev->cdev,devno,1);
	if(err)
		printk(KERN_NOTICE "Error %d adding scull%d",err,index);
}
static int scull_init_module(void)
{
	
	int result,i;
	dev_t dev = 0;
	//若主设备号非零,则直接注册
	if(scull_major){
		dev = MKDEV(scull_major,scull_minor);
		result = register_chrdev_region(dev,scull_nr_devs,"scull");
	}else{//否则动态分配主设备号
		result = alloc_chrdev_region(&dev,scull_minor,scull_nr_devs,"scull");
		scull_major = MAJOR(dev);
	}
	if(result < 0){
		printk(KERN_WARNING "scull : can't get major %d\n",scull_major);
		return result;
	}
	//分配多个设备scull_nr_devs
	scull_devices = kmalloc(scull_nr_devs*sizeof(struct scull_dev),GFP_KERNEL);
	if(!scull_devices){
		result = -ENOMEM;
		goto fail;	
	}
	memset(scull_devices,0,scull_nr_devs*sizeof(struct scull_dev));
	
	for(i=0;i<scull_nr_devs;++i){
		scull_devices[i].quantum = scull_quantum;
		scull_devices[i].qset = scull_qset;
		init_MUTEX(&scull_devices[i].sem);//初始化互斥信号量sem
		scull_setup_cdev(&scull_devices[i],i);//初始化字符设备
	}
	return 0;
	fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
