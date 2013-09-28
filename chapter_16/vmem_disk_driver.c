#include "vmem_disk_driver.h"


//定义some parameter and export to the symbol table
static int vmem_disk_major = 0;
module_param(vmem_disk_major,int,0);//the last parameter  means it can be visibile or not in sysfs
static int hardsect_size = 512;//扇区大小
module_param(hardsect_size,int,0);
static int nsectors = 1024;/*该驱动器扇区的数目*/
module_param(nsectors,int,0);
static int ndevices = 4;//分区数量
module_param(ndevices,int,0);
static int request_mode = RM_SIMPLE;
module_param(request_mode,int,0);

MODULE_AUTHOR("jefby");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("block driver");



static struct vmem_disk_dev *devices;//全局数据指针

//block_device_operations结构体
static struct block_device_operations vmem_disk_ops = {
.owner = THIS_MODULE,
.open = vmem_disk_open,
.release = vmem_disk_release,
.media_changed = vmem_disk_media_changed,
.revalidate_disk = vmem_disk_revalidate,
.getgeo = vmem_disk_getgeo,
};

/*
 * 初始化函数
 */
static int __init vmem_disk_init(void)
{
  int i;
	/*
 	* 注册块设备，能获得主设备号，但不能让系统使用任何磁盘
	* register_blkdev(major,name) major==0,分配一个新的主设备号给设备,
	* 并将该设备号返回给调用者,返回负值，表示出现了一个错误，0值不用
 	*/
	vmem_disk_major = register_blkdev(vmem_disk_major,"vmem_disk");
	if(vmem_disk_major <= 0){
	printk(KERN_WARNING "vmem_disk:unable to get major number\n");
	return -EBUSY;
	}
	
	//分配设备数组，并初始化
	devices =(struct vmem_disk_dev*)kmalloc(ndevices*sizeof(struct vmem_disk_dev),GFP_KERNEL);
	if(devices == NULL)
		goto out_unregister;
	for(i=0;i<ndevices;++i)
		setup_device(devices+i,i);
	return 0;
	out_unregister:
	//注销块设备驱动程序，必须与register_blkdev参数匹配
	unregister_blkdev(vmem_disk_major,"vmem_disk");
	return -ENOMEM;	
}
/*
 *注销函数
 */
static void vmem_disk_exit(void)
{
	int i;
	for(i=0;i<ndevices;++i){
		struct vmem_disk_dev *dev = devices+i;
		//如果有任何"介质移除"计时器处于活动状态,删除介质移除定时器
		del_timer_sync(&dev->timer);
		if(dev->gd){
			//卸载磁盘
			del_gendisk(dev->gd);
			//减少计数值
			put_disk(dev->gd);
		}
		if(dev->queue){
			if(request_mode ==RM_NOQUEUE)
				kobject_put(&dev->queue->kobj);
			else
				blk_cleanup_queue(dev->queue);
		}
		if(dev->data)
			vfree(dev->data);
	}
	unregister_blkdev(vmem_disk_major,"vmem_disk");
	kfree(devices);
}
/*
 *
 * 设置分区
 *
 */
void setup_device(struct vmem_disk_dev * dev,int which)
{
	/*
 	*初始化
 	*/
	memset(dev,0,sizeof(struct vmem_disk_dev));
	//设备的大小为扇区数*每个扇区的字节数
	dev->size = nsectors*hardsect_size;
	//分配数据，vmalloc用于分配比较大的内存，一般是虚拟地址空间的连续区域，尽管可能物理上不连续，错误是返回0
	dev->data = vmalloc(dev->size);
	if(dev->data == NULL){
	printk(KERN_NOTICE "vmalloc failure.\n");
	return;
	}	
	//初始化自旋锁
	spin_lock_init(&dev->lock);
	/*
 	*使用一个timer来模拟设备invalidate
 	*/
	init_timer(&dev->timer);
	dev->timer.data = (unsigned long)dev;
	dev->timer.function = vmem_disk_invalidate;
	/*
	*I/O队列,具体实现依赖于我们是否使用make_request函数
	*/
	switch(request_mode){
	case RM_NOQUEUE:
	/*
 	*请求队列分配
 	*/
	dev->queue = blk_alloc_queue(GFP_KERNEL);
	if(dev->queue == NULL)
	goto out_vfree;
	/*绑定"制造请求"*/
	blk_queue_make_request(dev->queue,vmem_disk_make_request);
	break;
	case RM_FULL:
	//创建和初始化请求队列
	dev->queue = blk_init_queue(vmem_disk_full_request,&dev->lock);
	if(dev->queue==NULL)
		goto out_vfree;
	break;
	default:
	printk(KERN_NOTICE "Bad request mode %d,using simple\n",request_mode);
	case RM_SIMPLE:
	//分配并初始化请求队列,vmem_disk_request是请求函数，用于执行块设备的读写请求,dev->lock用于控制对队列的访问
	dev->queue = blk_init_queue(vmem_disk_request,&dev->lock);
	if(dev->queue == NULL)
		goto out_vfree;
	break;
	}
	//设置扇区大小	
	blk_queue_logical_block_size(dev->queue,hardsect_size);

	dev->queue->queuedata = dev;
	/*gendisk分配与初始化，vmem_disk_MINORS是分区数量*/
	dev->gd = alloc_disk(vmem_disk_MINORS);
	if(!dev->gd){
	printk(KERN_NOTICE "alloc_disk failure\n");
	goto out_vfree;
	}
	//设置参数
	dev->gd->major = vmem_disk_major;
	dev->gd->first_minor = which*vmem_disk_MINORS;
	dev->gd->fops = &vmem_disk_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	//设置该磁盘设备的名称
	snprintf(dev->gd->disk_name,32,"vmem_disk%c",which+'a');
	//设置该磁盘设备包含的扇区数
	set_capacity(dev->gd,nsectors*(hardsect_size/KERNEL_SECTOR_SIZE));
	//激活磁盘设备
	add_disk(dev->gd);
	return;
	out_vfree:
	if(dev->data)
		vfree(dev->data);

}
//invalidate()在定时器到期时执行，设置一个标志来模拟磁盘的移除
void vmem_disk_invalidate(unsigned long ldev)
{
	struct vmem_disk_dev *dev = (struct vmem_disk_dev *)ldev;
	spin_lock(&dev->lock);
	if(dev->users || !dev->data)
		printk(KERN_WARNING "vmem_disk:timer sanity check failed!\n");
	else
		dev->media_change = 1;
	spin_unlock(&dev->lock);
}
//获得驱动器信息
static int vmem_disk_getgeo(struct block_device *bdev,struct hd_geometry *geo)
{
	long size;
	struct vmem_disk_dev *dev = bdev->bd_disk->private_data;
	
	size = dev->size*(hardsect_size/KERNEL_SECTOR_SIZE);
	geo->cylinders = (size & ~0x3f) >> 6;
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 4;

	return 0;
}


static int vmem_disk_open(struct block_device *bdev,fmode_t mode)
{
	struct vmem_disk_dev *dev = bdev->bd_disk->private_data;
	//删除介质移除定时器
	del_timer_sync(&dev->timer);
	spin_lock(&dev->lock);
	if(!dev->users)
		check_disk_change(bdev);//检查设备是否锁定
	dev->users++;//增加用户计数
	spin_unlock(&dev->lock);
	return 0;
}

static int vmem_disk_release(struct gendisk *disk,fmode_t mode)
{
	struct vmem_disk_dev *dev = disk->private_data;
	spin_lock(&dev->lock);
	dev->users--;//减少用户计数
	if(!dev->users){
	//启动介质移除定时器
		dev->timer.expires = jiffies + INVALIDATE_DELAY;
		add_timer(&dev->timer);
	}	
	spin_unlock(&dev->lock);
	return 0;
}

static int vmem_disk_media_changed(struct gendisk *disk)
{
	struct vmem_disk_dev *dev = disk->private_data;
	
	return dev->media_change;
}

static int vmem_disk_revalidate(struct gendisk *disk)
{
	struct vmem_disk_dev *dev = disk->private_data;
	if(dev->media_change){
		dev->media_change = 0;
		memset(dev->data,0,dev->size);
	}
	return 0;
}

//处理一个I/O request
//数据移动,req->sector：开始扇区的索引号,req->current_nr_sectors:需要传输的扇区数 buffer:要传输或者要接收数据的缓冲区指针
//该指针在内核虚拟地址中，如果有需要，内核可以直接引用它 req_data_dir:这个宏从request中得到传输的方向，0表示从设备读，非零表示
//向设备写数据

static void vmem_disk_transfer(struct vmem_disk_dev *dev,unsigned long sector,unsigned long nsect,char *buffer,int write)
{
	unsigned long offset = sector*KERNEL_SECTOR_SIZE;
	unsigned long nbytes = nsect*KERNEL_SECTOR_SIZE;
	if((offset+nbytes) > dev->size){
	printk(KERN_NOTICE "Beyond-end write (%ld %ld)\n",offset,nbytes);
	return;
	}
	if(write)
		memcpy(dev->data+offset,buffer,nbytes);
	else
		memcpy(buffer,dev->data+offset,nbytes);
}

//request函数的简单实现，一次一条
static void vmem_disk_request(struct request_queue *q)
{
	struct request *req;
	//获取队列中第一个未完成的请求（由I/O调度器决定），当没有请求需要处理时，该函数返回NULL
	while((req=elv_next_request(q)) != NULL){
		struct vmem_disk_dev *dev = req->rq_disk->private_data;
		//告诉用户该请求是否是一个文件系统请求--移动块数据的请求
		if(!blk_fs_request(req)){
			printk(KERN_NOTICE "Skip non-fs request\n");
			end_request(req,0);//传递0表示不能成功的完成该请求
			continue;
		}
		//数据移动,req->sector：开始扇区的索引号,req->current_nr_sectors:需要传输的扇区数 buffer:要传输或者要接收数据的缓冲区指针
		//该指针在内核虚拟地址中，如果有需要，内核可以直接引用它 req_data_dir:这个宏从request中得到传输的方向，0表示从设备读，非零表示
		//向设备写数据
		vmem_disk_transfer(dev,req->sector,req->current_nr_sectors,req->buffer,rq_data_dir(req));
		end_request(req,1);
	}
}

/*传输一个单独的BIO*/
static int vmem_disk_xfer_bio(struct vmem_disk_dev *dev,struct bio *bio)
{
	int i;
	struct bio_vec *bvec;
	sector_t sector = bio->bi_sector;
	
	//do each segment independently
	bio_for_each_segment(bvec,bio,i){
		//需要直接访问也，需要保证正确的内核虚拟地址是存在的，函数__bio_kmap_atomic直接映射了指定索引号为i的bio_vec中的缓冲区
		char *buffer = __bio_kmap_atomic(bio,i,KM_USER0);
		vmem_disk_transfer(dev,sector,bio_cur_bytes(bio)>>9,buffer,bio_data_dir(bio) == WRITE);
		sector += bio_cur_bytes(bio)>>9;
		__bio_kunmap_atomic(bio,KM_USER0);
	}
	return 0;//always "succeed"
}

/*
 * 传输一个完整的request
 * 返回传输的扇区数
 */
static int vmem_disk_xfer_request(struct vmem_disk_dev *dev,struct request *req)
{
	struct bio *bio;
	int nsect = 0;

	rq_for_each_bio(bio,req){
		vmem_disk_xfer_bio(dev,bio);//传输一个bio
		nsect += bio->bio_size / KERNEL_SECTOR_SIZE;//增加传输的扇区数目
	}
	return nsect;
}
/*
 *更强大的request
 */
static void vmem_disk_full_request(struct request_queue *q)
{
	struct request *req;
	int sectors_xferred;
	struct vmem_disk_dev *dev = q->queuedata;
	
	while((req=elv_next_request(q)) != NULL){//返回队列中需要处理的下一个请求指针
		if(!blk_fs_request(req)){//查看是否是有效的传输数据请求
			printk(KERN_NOTICE "Skip non-fs request\n");
			end_request(req,0);
			continue;
		}
		
		sectors_xferred = vmem_disk_xfer_request(dev,req);//传输sectors_xferred个扇区
		if(!end_that_request_first(req,1,sectors_xferred)){//驱动程序从上次结束的地方开始，传输了sectors_xferred个扇区
			blkdev_dequeue_request(req);//将请求从队列中删除
			end_that_request_last(req);//通知任何等待已经完成请求的对象
		}
	}
}

/*
 *	功能:制造请求方式,用于无队列模式，从块设备层中直接接受请求
 *	直接进行传输或者把请求重定向给其他设备
 *	@q:请求队列
 * 	@bio:要被传输一个或者多个缓冲区
 */
static int vmem_disk_make_request(struct request_queue *q,struct bio *bio)
{
	struct vmem_disk_dev *dev = q->queuedata;
	int status;
	//传输一个单独的BIO
	status = vmem_disk_xfer_bio(dev,bio);
	//在整个bio块上结束传输,status是返回值,status为0，表示成功，非0值表示错误
	bio_endio(bio,bio->bio_size,status);
	return 0;
}

module_init(vmem_disk_init);
module_exit(vmem_disk_exit);
