#ifndef _SCULLC_H
#define _SCULLC_H

#undef PDEBUG
#ifdef SCULLC_DEBUG
	#ifdef __KERNEL__
	#define PDEBUG(fmt,args...) printk(KERN_DEBUG "scullc:"fmt,##args)
	#else
	#define PDEBUG(fmt,args...) fprintf(stderr,fmt,##args)
	#endif
#else
#define PDEBUG(fmt,args...)//not debuging,nothing
#endif


#ifndef SCULLC_MAJOR
#define SCULLC_MAJOR 0 //dynamic major by default
#endif

#ifndef SCULLC_NR_DEVS 
#define SCULLC_NR_DEVS 4
#endif

#ifndef SCULLC_QUANTUM
#define SCULLC_QUANTUM 4000
#endif

#ifndef SCULLC_QSET
#define SCULLC_QSET 1000
#endif

#include <linux/ioctl.h>//_IO

//use 'K' as the magic number
#define SCULLC_IOC_MAGIC	'K'


#define SCULLC_IOCRESET _IO(SCULLC_IOC_MAGIC,0)



#define SCULLC_IOCSQUANTUM	_IOW(SCULLC_IOC_MAGIC,1,int)
#define SCULLC_IOCSQSET 		_IOW(SCULLC_IOC_MAGIC,2,int)
#define SCULLC_IOCTQUANTUM 	_IO(SCULLC_IOC_MAGIC,3)
#define SCULLC_IOCTQSET 		_IO(SCULLC_IOC_MAGIC,4)
#define SCULLC_IOCGQUANTUM 	_IOR(SCULLC_IOC_MAGIC,5,int)
#define SCULLC_IOCGQSET		_IOR(SCULLC_IOC_MAGIC,6,int)
#define SCULLC_IOCQQUANTUM	_IO(SCULLC_IOC_MAGIC,7)
#define SCULLC_IOCQQSET		_IO(SCULLC_IOC_MAGIC,8)
#define SCULLC_IOCXQUANTUM	_IOWR(SCULLC_IOC_MAGIC,9,int)
#define SCULLC_IOCXQSET		_IOWR(SCULLC_IOC_MAGIC,10,int)
#define SCULLC_IOCHQUANTUM	_IO(SCULLC_IOC_MAGIC,11)
#define SCULLC_IOCHQSET 		_IO(SCULLC_IOC_MAGIC,12)

#define SCULLC_IOC_MAXNR		12


/*
* Representation of scullc quantum sets
*/
struct scullc_dev{
	void **data;
	struct scullc_dev *next;//next listitem
	int vmas;	//active maping 
	int quantum;//
	int qset; // the current array size
	size_t size;//amount of data stored here
	struct semaphore sem;//mutual exclusion sempahore
	struct cdev cdev;//char device structure
};
extern struct scullc_dev *scullc_devices;
extern struct file_operations scullc_fops;
//the different configurable parameters
extern int scullc_major;
extern int scullc_minor;
extern int scullc_quantum;
extern int scullc_qset;

int scullc_trim(struct scullc_dev *dev);
ssize_t scullc_read(struct file*filp,char __user *buf,size_t count,loff_t *f_pos);
ssize_t scullc_write(struct file*filp,const char __user *buf,size_t count,loff_t *f_pos);
loff_t scullc_llseek(struct file*filp,loff_t off,int whence);
//int scullc_ioctl(struct inode*inode,struct file*filp,unsigned int cmd,unsigned long arg);


#endif/*_SCULLC_H*/
