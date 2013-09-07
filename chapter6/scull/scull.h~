#ifndef _SCULL_H
#define _SCULL_H

#ifndef SCULL_MAJOR
#define SCULL_MAJOR 0 //dynamic major by default
#endif

#ifndef SCULL_NR_DEVS 
#define SCULL_NR_DEVS 4
#endif

#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM 4000
#endif

#ifndef SCULL_QSET
#define SCULL_QSET 1000
#endif

#include <linux/ioctl.h>//_IO

//use 'K' as the magic number
#define SCULL_IOC_MAGIC	'K'


#define SCULL_IOCRESET _IO(SCULL_IOC_MAGIC,0)



#define SCULL_IOCSQUANTUM	_IOW(SCULL_IOC_MAGIC,1,int)
#define SCULL_IOCSQSET 		_IOW(SCULL_IOC_MAGIC,2,int)
#define SCULL_IOCTQUANTUM 	_IO(SCULL_IOC_MAGIC,3)
#define SCULL_IOCTQSET 		_IO(SCULL_IOC_MAGIC,4)
#define SCULL_IOCGQUANTUM 	_IOR(SCULL_IOC_MAGIC,5,int)
#define SCULL_IOCGQSET		_IOR(SCULL_IOC_MAGIC,6,int)
#define SCULL_IOCQQUANTUM	_IO(SCULL_IOC_MAGIC,7)
#define SCULL_IOCQQSET		_IO(SCULL_IOC_MAGIC,8)
#define SCULL_IOCXQUANTUM	_IOWR(SCULL_IOC_MAGIC,9,int)
#define SCULL_IOCXQSET		_IOWR(SCULL_IOC_MAGIC,10,int)
#define SCULL_IOCHQUANTUM	_IO(SCULL_IOC_MAGIC,11)
#define SCULL_IOCHQSET 		_IO(SCULL_IOC_MAGIC,12)

#define SCULL_IOC_MAXNR		14


/*
* Representation of scull quantum sets
*/
struct scull_qset{
	void **data;
	struct scull_qset *next;
};

struct scull_dev{
	struct scull_qset *data;//pointer to first quantum set
	int quantum;//
	int qset;
	unsigned long size;//amount of data stored here
	unsigned int access_key;//used by sculluid and scullpriv
	struct semaphore sem;//mutual exclusion sempahore
	struct cdev cdev;//char device structure
};
//the different configurable parameters
extern int scull_major;
extern int scull_minor;
extern int scull_quantum;
extern int scull_qset;

int scull_trim(struct scull_dev *dev);
ssize_t scull_read(struct file*filp,char __user *buf,size_t count,loff_t *f_pos);
ssize_t scull_write(struct file*filp,const char __user *buf,size_t count,loff_t *f_pos);
loff_t scull_llseek(struct file*filp,loff_t off,int whence);
//int scull_ioctl(struct inode*inode,struct file*filp,unsigned int cmd,unsigned long arg);


#endif/*_SCULL_H*/
