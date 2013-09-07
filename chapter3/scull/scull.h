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


#endif
