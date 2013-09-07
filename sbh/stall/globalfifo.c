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

#define GLOBALFIFO_SIZE 0x1000 //ȫ������ڴ�
#define MEM_CLEAR 0x1	//��0
#define GLOBALFIFO_MAJOR 0//Ԥ������豸��

static int globalfifo_major = GLOBALFIFO_MAJOR;

//globalfifo�豸�ṹ��
struct globalfifo_dev
{
	struct cdev cdev;//cdev �ṹ��
	unsigned int current_len;//fifo��Ч���ݳ���
	unsigned char mem[GLOBALFIFO_SIZE];//ȫ���ڴ�
	struct semaphore sem;//���������õ��ź���
	wait_queue_head_t r_wait;//�������õĵȴ�����ͷ
	wait_queue_head_t w_wait;//����д�õĵȴ�����ͷ
};

struct globalfifo_dev *devp;

int globalfifo_open(struct inode *inode,struct file *filp)
{
	//���豸�ṹ���ָ�븳ֵ���ļ�˽������ָ��
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
//		if(down_interruptible(&devp->sem))//����ź���
//			return -ERESTARTSYS;
		memset(devp->mem,0,GLOBALFIFO_SIZE);
//		up(&devp->sem);//�ͷ��ź��� 
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
	
	DECLARE_WAITQUEUE(wait,current);//����ȴ�����

//	if(p >= GLOBALFIFO_SIZE)
//		return 0;
	down(&devp->sem);//����ź���
	add_wait_queue(&devp->r_wait,&wait);//������ȴ�����
	//�ȴ�FIFO�ǿ�
	while(devp->current_len == 0){
		if(filp->f_flags & O_NONBLOCK){//��������д
			ret = -EAGAIN;
			goto out;
		}
	
	__set_current_state(TASK_INTERRUPTIBLE);//�ı����״̬Ϊ˯��
	up(&devp->sem);//�ͷ��ź���

	schedule();//������������ִ��
	if(signal_pending(current)){//�������Ϊ�źŻ���
		ret = -ERESTARTSYS;
		goto out2;
	}
	down(&devp->sem);//����ź���
	}

	if(count > devp->current_len)//�������û��ռ�
		count = devp->current_len;
	if(copy_to_user(buf,(void *)(devp->mem),count)){
		ret = -EFAULT;
		goto out;
	}
	else{
		memcpy(devp->mem,devp->mem+count,devp->current_len-count);//fifo����ǰ��
		devp->current_len -=count;
		printk(KERN_INFO "read %d bytes from %d\n",count,devp->current_len);
		wake_up_interruptible(&devp->w_wait);//����д�ȴ�����
		ret = count;
	}
	
	out:up(&devp->sem);//�ͷ��ź���
	out2:remove_wait_queue(&devp->r_wait,&wait);//�Ƴ��ȴ�����
	set_current_state(TASK_RUNNING);
	return ret;
}

static ssize_t globalfifo_write(struct file *filp,const char __user *buf,size_t count,loff_t *f_pos)
{
	struct globalfifo_dev *devp = filp->private_data;
	int ret = 0;
	DECLARE_WAITQUEUE(wait,current);//����ȴ�����

	down(&devp->sem);//��ȡ�ź���
	add_wait_queue(&devp->w_wait,&wait);//����д�ȴ�����ͷ
	while(devp->current_len == GLOBALFIFO_SIZE){
		if(filp->f_flags &O_NONBLOCK){//����Ƿ���������
			ret = -EAGAIN;
			goto out;
		}
	__set_current_state(TASK_INTERRUPTIBLE);//�ı����״̬Ϊ˯��
	up(&devp->sem);
	schedule();//������������ִ��
	if(signal_pending(current)){//�������Ϊ�źŻ���
		ret = -ERESTARTSYS;
		goto out2;
	}
	down(&devp->sem);//����ź���
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
		wake_up_interruptible(&devp->r_wait);//���Ѷ��ȴ�����
		ret = count;
	}
	out:up(&devp->sem);//�ͷ��ź���
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
	case 1://��ǰλ��
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
	case 2://��β
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
	//init_MUTEX(devp->sem);//��ʼ���ź���
	sema_init(&devp->sem,1);
	init_waitqueue_head(&devp->r_wait);//��ʼ�����ȴ�����ͷ
	init_waitqueue_head(&devp->w_wait);//��ʼ��д�ȴ�����ͷ
	printk(KERN_ALERT "dev major is %d , usage:sudo  mknod /dev/globalfifo c major minor",globalfifo_major);

	return 0;
fail_malloc:
	unregister_chrdev_region(devno,1);
	return result;
}

void globalfifo_exit(void)
{
	cdev_del(&devp->cdev);//ע��cdev
	kfree((void*)devp);//�ͷ��豸�ṹ���ڴ�
	unregister_chrdev_region(MKDEV(globalfifo_major,0),1);//�ͷ��豸��
}

module_init(globalfifo_init);
module_exit(globalfifo_exit);
