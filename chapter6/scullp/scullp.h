#ifndef _SCULLP_H
#define _SCULLP_H

#undef PDEBUG
#ifdef SCULLP_DEBUG
	#ifdef __KERNEL__
	#define PDEBUG(fmt,args...) printk(KERN_DEBUG "scullp:"fmt,##args)
	#else
	#define PDEBUG(fmt,args...) fprintf(stderr,fmt,##args)
	#endif
#else
#define PDEBUG(fmt,args...)//not debuging,nothing
#endif


#ifndef SCULLP_MAJOR
#define SCULLP_MAJOR 0 //dynamic major by default
#endif

#ifndef SCULLP_NR_DEVS 
#define SCULLP_NR_DEVS 4
#endif

#ifndef SCULLP_ORDER
#define SCULLP_ORDER 4000
#endif

#ifndef SCULLP_QSET
#define SCULLP_QSET 1000
#endif

#ifndef SCULLP_ORDER 
#define SCULLP_ORDER 0
#endif


#include <linux/ioctl.h>//_IO

//use 'K' as the magic number
#define SCULLP_IOC_MAGIC	'K'


#define SCULLP_IOCRESET _IO(SCULLP_IOC_MAGIC,0)



#define SCULLP_IOCSORDER	_IOW(SCULLP_IOC_MAGIC,1,int)
#define SCULLP_IOCSQSET 		_IOW(SCULLP_IOC_MAGIC,2,int)
#define SCULLP_IOCTORDER 	_IO(SCULLP_IOC_MAGIC,3)
#define SCULLP_IOCTQSET 		_IO(SCULLP_IOC_MAGIC,4)
#define SCULLP_IOCGORDER 	_IOR(SCULLP_IOC_MAGIC,5,int)
#define SCULLP_IOCGQSET		_IOR(SCULLP_IOC_MAGIC,6,int)
#define SCULLP_IOCQORDER	_IO(SCULLP_IOC_MAGIC,7)
#define SCULLP_IOCQQSET		_IO(SCULLP_IOC_MAGIC,8)
#define SCULLP_IOCXORDER	_IOWR(SCULLP_IOC_MAGIC,9,int)
#define SCULLP_IOCXQSET		_IOWR(SCULLP_IOC_MAGIC,10,int)
#define SCULLP_IOCHORDER	_IO(SCULLP_IOC_MAGIC,11)
#define SCULLP_IOCHQSET 		_IO(SCULLP_IOC_MAGIC,12)

#define SCULLP_IOC_MAXNR		12


/*
* Representation of scullp quantum sets
*/
struct scullp_dev{
	void **data;
	struct scullp_dev *next;//next listitem
	int vmas;	//active maping 
	int order;//size of pages
	int qset; // the current array size
	size_t size;//amount of data stored here
	struct semaphore sem;//mutual exclusion sempahore
	struct cdev cdev;//char device structure
};
extern struct scullp_dev *scullp_devices;
extern struct file_operations scullp_fops;
//the different configurable parameters
extern int scullp_major;
extern int scullp_minor;
extern int scullp_order;
extern int scullp_qset;

int scullp_trim(struct scullp_dev *dev);
ssize_t scullp_read(struct file*filp,char __user *buf,size_t count,loff_t *f_pos);
ssize_t scullp_write(struct file*filp,const char __user *buf,size_t count,loff_t *f_pos);
loff_t scullp_llseek(struct file*filp,loff_t off,int whence);
//int scullp_ioctl(struct inode*inode,struct file*filp,unsigned int cmd,unsigned long arg);


#endif/*_SCULLP_H*/
