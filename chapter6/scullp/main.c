#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>//dev_t MAJOR MINOR MKDEV
#include <linux/fs.h>//register_chrdev_region
#include <linux/cdev.h>//cdev
#include <linux/kernel.h>//container_of
#include <asm/uaccess.h>//copy_*_user

#include "scullp.h"


int scullp_major = SCULLP_MAJOR;
int scullp_minor = 0;
int scullp_nr_devs = SCULLP_NR_DEVS;
int scullp_qset = SCULLP_QSET;
int scullp_order = SCULLP_ORDER;

module_param(scullp_major,int,S_IRUGO);
module_param(scullp_minor,int,S_IRUGO);
module_param(scullp_nr_devs,int,S_IRUGO);
module_param(scullp_order,int,S_IRUGO);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("jefby");

int scullp_trim(struct scullp_dev *dev);
void scullp_cleanup(void);


struct scullp_dev *scullp_devices;
//declare one cache pointer:use it for all devices
kmem_cache_t *scullp_cache;

int scullp_trim(struct scullp_dev *dev)
{
	struct scullp_dev *dptr,*next;
	int qset = dev->qset;//dev is not-null
	int i;

	for(dptr = dev;dptr;dptr=next){
		if(dptr->data){
			for(i=0;i<qset;i++)
			//free the pages 
			if(dptr->data[i])
				free_pages((unsigned long)(dptr->data[i]),dptr->order);	
			kfree(dptr->data);
			dptr->data = NULL;
		}
		next = dptr->next;
		if(dptr!=dev)kfree(dptr);
	}
	dev->size = 0;
	dev->order = scullp_order;
	dev->qset = scullp_qset;
	dev->data = NULL;
	return 0;
}

int scullp_open(struct inode *inode,struct file *filp)
{
	struct scullp_dev *dev;//device information
	
	//Find the device
	dev = container_of(inode->i_cdev,struct scullp_dev,cdev);
	

	if((filp->f_flags & O_ACCMODE) == O_WRONLY){
		if(down_interruptible(&dev->sem))
			return -ERESTARTSYS;
		scullp_trim(dev);//ignore errors
		up(&dev->sem);
	}
	
	filp->private_data = dev;//for other methods
	return 0;//success
}

int scullp_release(struct inode *inode,struct file *filp)
{
	return 0;
}

struct scullp_dev* scullp_follow(struct scullp_dev *dev,int n)
{
	while(n--){
		if(!dev->next){
			dev->next = kmalloc(sizeof(struct scullp_dev),GFP_KERNEL);
			if(dev->next == NULL)
				return NULL;
			memset(dev->next,0,sizeof(struct scullp_dev));
		}
		dev = dev->next;
		continue;
	}
	return dev;
}

ssize_t scullp_read(struct file *filp,char __user *buf,size_t count,loff_t *f_pos)
{
	struct scullp_dev *dev=filp->private_data;
	struct scullp_dev *dptr;
	int order = dev->order,qset = dev->qset;
	int itemsize = order*qset;//
	int item,s_pos,q_pos,rest;

	ssize_t retval = 0;

	if(down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	if(*f_pos>=dev->size)
		goto out;
	if(*f_pos+count > dev->size)
		count = dev->size - *f_pos;

	item = (long)*f_pos/itemsize;
	rest = (long)*f_pos%itemsize;
	
	s_pos = rest/order;
	q_pos = rest%order;

	//follow the list up to the right postion
	dptr = scullp_follow(dev,item);
	if( !dptr->data || !dptr->data[s_pos])
		goto out;	
	if(count > order - q_pos)
		count = order - q_pos;
	if(copy_to_user(buf,dptr->data[s_pos]+q_pos,count)){
		retval = -EFAULT;
		goto out;
	}

	*f_pos += count;
	retval = count;
	out:
	up(&dev->sem);
	return retval;
}
ssize_t scullp_write(struct file*filp,const char __user *buf,size_t count,loff_t *f_pos)
{
	struct scullp_dev *dev = filp->private_data;
	struct scullp_dev *dptr;
	int order = dev->order,qset=dev->qset;
	int itemsize = order * qset;
	int item,s_pos,q_pos,rest;
	ssize_t retval = -ENOMEM;
	if(down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	
	item = (long)*f_pos/itemsize;
	rest = (long)*f_pos%itemsize;

	s_pos = rest / order;
	q_pos = rest % order;
	dptr = scullp_follow(dev,item);
	
	if(!dptr->data){
		dptr->data = kmalloc(qset*sizeof(char*),GFP_KERNEL);
		if(!dptr->data)
			goto out;
		memset(dptr->data,0,sizeof(char*)*qset);
	}
	if(!dptr->data[s_pos]){
		//use get_free_pages alloc 2^dptr-order pages
		dptr->data[s_pos] = (void*)__get_free_pages(GFP_KERNEL,dptr->order); 
		if(!dptr->data[s_pos])
			goto out;
		memset(dptr->data[s_pos],0,PAGE_SIZE << dptr->order);
	}
	if(count > order - q_pos)
		count = order - q_pos;
	if(copy_from_user(dptr->data[s_pos]+q_pos,buf,count)){
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	retval = count;
	if(dev->size < *f_pos)
		dev->size = *f_pos;
	out:
	up(&dev->sem);
	return retval;
}
loff_t scullp_llseek(struct file *filp,loff_t off,int whence)
{
	struct scullp_dev *dev=filp->private_data;
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
int scullp_ioctl(struct inode *inode,struct file *filp,unsigned int cmd,unsigned long arg)
{
	int err=0,tmp;
	int retval = 0;

	if(_IOC_TYPE(cmd) != SCULLP_IOC_MAGIC)	return -ENOTTY;
	if(_IOC_NR(cmd) > SCULLP_IOC_MAXNR)	return -ENOTTY;

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
	case SCULLP_IOCRESET:
		scullp_order = SCULLP_ORDER;
		scullp_qset = SCULLP_QSET;
		break;
	case SCULLP_IOCSORDER:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		retval=__get_user(scullp_order,(int __user*)arg);
		break;
	case SCULLP_IOCSQSET:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		retval=__get_user(scullp_qset,(int __user*)arg);
		break;
	case SCULLP_IOCTORDER:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		scullp_order = arg;
		break;
	case SCULLP_IOCGORDER:
		retval=put_user(scullp_order,(int __user*)arg);
		break;
	case SCULLP_IOCQORDER:
		return scullp_order;
	case SCULLP_IOCXORDER:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scullp_order;
		retval = __get_user(scullp_order,(int __user*)arg);
		if(retval == 0)
			retval = __put_user(tmp,(int __user*)arg);
		break;
	case SCULLP_IOCHORDER:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scullp_order;
		scullp_order = arg;
		return tmp;
	case SCULLP_IOCTQSET:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		scullp_qset = arg;
		break;
	case SCULLP_IOCGQSET:
		retval = __put_user(scullp_qset,(int __user*)arg);
		break;
	case SCULLP_IOCQQSET:
		return scullp_qset;
	case SCULLP_IOCXQSET:
		if(!capable(CAP_SYS_ADMIN)) 
			return -EPERM;
		tmp = scullp_qset;
		retval = __get_user(scullp_qset,(int __user*)arg);
		if(retval == 0)
			retval = put_user(tmp,(int __user*)arg);
		break;
	case SCULLP_IOCHQSET:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scullp_qset;
		scullp_qset = arg;
		return tmp;
	default:
		return -ENOTTY;
	}
	return retval;


}
struct file_operations scullp_fops={
	.owner = THIS_MODULE,
	.llseek = scullp_llseek,
	.read = scullp_read,
	.write = scullp_write,
	.open = scullp_open,
	.release = scullp_release,
	.ioctl = scullp_ioctl,
};
static void scullp_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(scullp_major,scullp_minor);
	if(scullp_devices){
		for(i=0;i<scullp_nr_devs;i++){
			scullp_trim(scullp_devices+i);
			cdev_del(&scullp_devices[i].cdev);
		}
		kfree(scullp_devices);
	}
	
	//clean_up module is never called if registering failed
	unregister_chrdev_region(devno,scullp_nr_devs);
}
static void scullp_setup_cdev(struct scullp_dev *dev,int index)
{
	int err,devno=MKDEV(scullp_major,scullp_minor+index);

	cdev_init(&dev->cdev,&scullp_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scullp_fops;
	err = cdev_add(&dev->cdev,devno,1);
	/*Fail gracefully if need be*/
	if(err)
		printk(KERN_NOTICE "Error %d adding scullp%d",err,index);
}
static int scullp_init_module(void)
{
	
	int result,i;
	dev_t dev = 0;
	if(scullp_major){
		dev = MKDEV(scullp_major,scullp_minor);
		result = register_chrdev_region(dev,scullp_nr_devs,"scullp");
	}else{
		result = alloc_chrdev_region(&dev,scullp_minor,scullp_nr_devs,"scullp");
		scullp_major = MAJOR(dev);
	}
	if(result < 0){
		printk(KERN_WARNING "scullp : can't get major %d\n",scullp_major);
		return result;
	}

	scullp_devices = kmalloc(scullp_nr_devs*sizeof(struct scullp_dev),GFP_KERNEL);
	if(!scullp_devices){
		result = -ENOMEM;
		goto fail;	
	}
	memset(scullp_devices,0,scullp_nr_devs*sizeof(struct scullp_dev));
	
	for(i=0;i<scullp_nr_devs;++i){
		scullp_devices[i].order = scullp_order;
		scullp_devices[i].qset = scullp_qset;
		init_MUTEX(&scullp_devices[i].sem);
		scullp_setup_cdev(&scullp_devices[i],i);
	}
	return 0;
	fail:
	unregister_chrdev_region(dev,scullp_nr_devs);
	return result;
}

module_init(scullp_init_module);
module_exit(scullp_cleanup_module);
