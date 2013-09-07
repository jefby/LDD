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

#include "scullc.h"


int scullc_major = SCULLC_MAJOR;
int scullc_minor = 0;
int scullc_nr_devs = SCULLC_NR_DEVS;
int scullc_qset = SCULLC_QSET;
int scullc_quantum = SCULLC_QUANTUM;

module_param(scullc_major,int,S_IRUGO);
module_param(scullc_minor,int,S_IRUGO);
module_param(scullc_nr_devs,int,S_IRUGO);
module_param(scullc_quantum,int,S_IRUGO);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("jefby");

int scullc_trim(struct scullc_dev *dev);
void scullc_cleanup(void);


struct scullc_dev *scullc_devices;
//declare one cache pointer:use it for all devices
kmem_cache_t *scullc_cache;

int scullc_trim(struct scullc_dev *dev)
{
	struct scullc_dev *dptr,*next;
	int qset = dev->qset;//dev is not-null
	int i;

	for(dptr = dev;dptr;dptr=next){
		if(dptr->data){
			for(i=0;i<qset;i++)
			//free the scullc_cache 
			if(dptr->data[i])
				kmem_cache_free(scullc_cache,dptr->data[i]);
			kfree(dptr->data);
			dptr->data = NULL;
		}
		next = dptr->next;
		if(dptr!=dev)kfree(dptr);
	}
	dev->size = 0;
	dev->quantum = scullc_quantum;
	dev->qset = scullc_qset;
	dev->data = NULL;
	return 0;
}

int scullc_open(struct inode *inode,struct file *filp)
{
	struct scullc_dev *dev;//device information
	
	//Find the device
	dev = container_of(inode->i_cdev,struct scullc_dev,cdev);
	

	if((filp->f_flags & O_ACCMODE) == O_WRONLY){
		if(down_interruptible(&dev->sem))
			return -ERESTARTSYS;
		scullc_trim(dev);//ignore errors
		up(&dev->sem);
	}
	
	filp->private_data = dev;//for other methods
	return 0;//success
}

int scullc_release(struct inode *inode,struct file *filp)
{
	return 0;
}

struct scullc_dev* scullc_follow(struct scullc_dev *dev,int n)
{
	while(n--){
		if(!dev->next){
			dev->next = kmalloc(sizeof(struct scullc_dev),GFP_KERNEL);
			if(dev->next == NULL)
				return NULL;
			memset(dev->next,0,sizeof(struct scullc_dev));
		}
		dev = dev->next;
		continue;
	}
	return dev;
}

ssize_t scullc_read(struct file *filp,char __user *buf,size_t count,loff_t *f_pos)
{
	struct scullc_dev *dev=filp->private_data;
	struct scullc_dev *dptr;
	int quantum = dev->quantum,qset = dev->qset;
	int itemsize = quantum*qset;//
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
	
	s_pos = rest/quantum;
	q_pos = rest%quantum;

	//follow the list up to the right postion
	dptr = scullc_follow(dev,item);
	if( !dptr->data || !dptr->data[s_pos])
		goto out;	
	if(count > quantum - q_pos)
		count = quantum - q_pos;
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
ssize_t scullc_write(struct file*filp,const char __user *buf,size_t count,loff_t *f_pos)
{
	struct scullc_dev *dev = filp->private_data;
	struct scullc_dev *dptr;
	int quantum = dev->quantum,qset=dev->qset;
	int itemsize = quantum * qset;
	int item,s_pos,q_pos,rest;
	ssize_t retval = -ENOMEM;
	if(down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	
	item = (long)*f_pos/itemsize;
	rest = (long)*f_pos%itemsize;

	s_pos = rest / quantum;
	q_pos = rest % quantum;
	dptr = scullc_follow(dev,item);
	
	if(!dptr->data){
		dptr->data = kmalloc(qset*sizeof(char*),GFP_KERNEL);
		if(!dptr->data)
			goto out;
		memset(dptr->data,0,sizeof(char*)*qset);
	}
	//allocate a quantum using the memeory cache
	if(!dptr->data[s_pos]){
		dptr->data[s_pos] = kmem_cache_alloc(scullc_cache,GFP_KERNEL); 
		if(!dptr->data[s_pos])
			goto out;
		memset(dptr->data[s_pos],0,scullc_quantum);
	}
	if(count > quantum - q_pos)
		count = quantum - q_pos;
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
loff_t scullc_llseek(struct file *filp,loff_t off,int whence)
{
	struct scullc_dev *dev=filp->private_data;
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
int scullc_ioctl(struct inode *inode,struct file *filp,unsigned int cmd,unsigned long arg)
{
	int err=0,tmp;
	int retval = 0;

	if(_IOC_TYPE(cmd) != SCULLC_IOC_MAGIC)	return -ENOTTY;
	if(_IOC_NR(cmd) > SCULLC_IOC_MAXNR)	return -ENOTTY;

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
	case SCULLC_IOCRESET:
		scullc_quantum = SCULLC_QUANTUM;
		scullc_qset = SCULLC_QSET;
		break;
	case SCULLC_IOCSQUANTUM:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		retval=__get_user(scullc_quantum,(int __user*)arg);
		break;
	case SCULLC_IOCSQSET:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		retval=__get_user(scullc_qset,(int __user*)arg);
		break;
	case SCULLC_IOCTQUANTUM:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		scullc_quantum = arg;
		break;
	case SCULLC_IOCGQUANTUM:
		retval=put_user(scullc_quantum,(int __user*)arg);
		break;
	case SCULLC_IOCQQUANTUM:
		return scullc_quantum;
	case SCULLC_IOCXQUANTUM:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scullc_quantum;
		retval = __get_user(scullc_quantum,(int __user*)arg);
		if(retval == 0)
			retval = __put_user(tmp,(int __user*)arg);
		break;
	case SCULLC_IOCHQUANTUM:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scullc_quantum;
		scullc_quantum = arg;
		return tmp;
	case SCULLC_IOCTQSET:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		scullc_qset = arg;
		break;
	case SCULLC_IOCGQSET:
		retval = __put_user(scullc_qset,(int __user*)arg);
		break;
	case SCULLC_IOCQQSET:
		return scullc_qset;
	case SCULLC_IOCXQSET:
		if(!capable(CAP_SYS_ADMIN)) 
			return -EPERM;
		tmp = scullc_qset;
		retval = __get_user(scullc_qset,(int __user*)arg);
		if(retval == 0)
			retval = put_user(tmp,(int __user*)arg);
		break;
	case SCULLC_IOCHQSET:
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scullc_qset;
		scullc_qset = arg;
		return tmp;
	default:
		return -ENOTTY;
	}
	return retval;


}
struct file_operations scullc_fops={
	.owner = THIS_MODULE,
	.llseek = scullc_llseek,
	.read = scullc_read,
	.write = scullc_write,
	.open = scullc_open,
	.release = scullc_release,
	.ioctl = scullc_ioctl,
};
static void scullc_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(scullc_major,scullc_minor);
	if(scullc_devices){
		for(i=0;i<scullc_nr_devs;i++){
			scullc_trim(scullc_devices+i);
			cdev_del(&scullc_devices[i].cdev);
		}
		kfree(scullc_devices);
	}
	
	if(scullc_cache)
		kmem_cache_destroy(scullc_cache);
	//clean_up module is never called if registering failed
	unregister_chrdev_region(devno,scullc_nr_devs);
}
static void scullc_setup_cdev(struct scullc_dev *dev,int index)
{
	int err,devno=MKDEV(scullc_major,scullc_minor+index);

	cdev_init(&dev->cdev,&scullc_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scullc_fops;
	err = cdev_add(&dev->cdev,devno,1);
	/*Fail gracefully if need be*/
	if(err)
		printk(KERN_NOTICE "Error %d adding scullc%d",err,index);
}
static int scullc_init_module(void)
{
	
	int result,i;
	dev_t dev = 0;
	if(scullc_major){
		dev = MKDEV(scullc_major,scullc_minor);
		result = register_chrdev_region(dev,scullc_nr_devs,"scullc");
	}else{
		result = alloc_chrdev_region(&dev,scullc_minor,scullc_nr_devs,"scullc");
		scullc_major = MAJOR(dev);
	}
	if(result < 0){
		printk(KERN_WARNING "scullc : can't get major %d\n",scullc_major);
		return result;
	}

	scullc_devices = kmalloc(scullc_nr_devs*sizeof(struct scullc_dev),GFP_KERNEL);
	if(!scullc_devices){
		result = -ENOMEM;
		goto fail;	
	}
	memset(scullc_devices,0,scullc_nr_devs*sizeof(struct scullc_dev));
	
	for(i=0;i<scullc_nr_devs;++i){
		scullc_devices[i].quantum = scullc_quantum;
		scullc_devices[i].qset = scullc_qset;
		init_MUTEX(&scullc_devices[i].sem);
		scullc_setup_cdev(&scullc_devices[i],i);
	}
	scullc_cache = kmem_cache_create("scullc",scullc_quantum,0,SLAB_HWCACHE_ALIGN,NULL,NULL);
	if(!scullc_cache){
		scullc_cleanup_module();
		return -ENOMEM;//no ctor/dtor
	}
	return 0;
	fail:
	unregister_chrdev_region(dev,scullc_nr_devs);
	return result;
}

module_init(scullc_init_module);
module_exit(scullc_cleanup_module);
