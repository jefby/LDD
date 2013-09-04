/*
 *  
 *	Author:jefby
 *	Email:jef199006@gmail.com
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

#include <linux/init.h> // for module_init or module_exit
#include <linux/module.h> // for MODULE_LICENSE,module_param
#include <linux/errno.h>//like errno.h
#include <linux/fs.h> //register_blkdev or block_device_operations
#include <linux/genhd.h> // gendisk
#include <linux/blkdev.h>//blk_init_queue or elv_next_request or end_request
#include <linux/bio.h> //struct bio or bio_data_dir or __bio_kunmap_atomic
#include <linux/spinlock.h> //spin_lock_init or spin_lock_t
#include <linux/timer.h>//timer_list 
#include <linux/hdreg.h>//hd_geometry

typedef struct request_queue request_queue_t;

#define INVALIDATE_DELAY        30*HZ
#define KERNEL_SECTOR_SIZE 512
#define vmem_disk_MINORS 1
/*
 *能使用的不同request模式
 */
enum{
RM_SIMPLE = 0,/*简单请求函数*/
RM_FULL = 1,//复杂的请求函数
RM_NOQUEUE = 2//使用make_request,不使用请求队列
};


struct vmem_disk_dev{
	unsigned char * data;//数据
        int size;//以扇区为单位。设备大小
	short users;//用户数目
	short media_change;//介质改变状态
	spinlock_t lock;//用于互斥
	struct request_queue * queue;//设备请求队列
	struct gendisk *gd;//gendisk 结构,表示独立的磁盘设备或者分区
	struct timer_list timer;//用来模拟设备介质改变
};

void setup_device(struct vmem_disk_dev * dev,int which);
//invalidate()在定时器到期时执行，设置一个标志来模拟磁盘的移除
void vmem_disk_invalidate(unsigned long ldev);
//获得驱动器信息
static int vmem_disk_getgeo(struct block_device *bdev,struct hd_geometry *geo);
static int vmem_disk_open(struct block_device *bdev,fmode_t mode);
static int vmem_disk_release(struct gendisk *disk,fmode_t mode);
static int vmem_disk_media_changed(struct gendisk *disk);
static int vmem_disk_revalidate(struct gendisk *disk);
//处理一个I/O request
////数据移动,req->sector：开始扇区的索引号,req->current_nr_sectors:需要传输的扇区数 buffer:要传输或者要接收数据的缓冲区指针
//该指针在内核虚拟地址中，如果有需要，内核可以直接引用它 req_data_dir:这个宏从request中得到传输的方向，0表示从设备读，非零表示
//向设备写数据
static void vmem_disk_transfer(struct vmem_disk_dev *dev,unsigned long sector,unsigned long nsect,char *buffer,int write);
//request函数的简单实现，一次一条
static void vmem_disk_request(struct request_queue *q);
/*传输一个单独的BIO*/
static int vmem_disk_xfer_bio(struct vmem_disk_dev *dev,struct bio *bio);

/*
 * 传输一个完整的request
 * 返回传输的扇区数
 */
static int vmem_disk_xfer_request(struct vmem_disk_dev *dev,struct request *req);
/*
 *更强大的request
 */
static void vmem_disk_full_request(struct request_queue *q);
/*
 *	功能:制造请求方式,用于无队列模式，从块设备层中直接接受请求
 *	直接进行传输或者把请求重定向给其他设备
 *	@q:请求队列
 * 	@bio:要被传输一个或者多个缓冲区
 */
static int vmem_disk_make_request(struct request_queue *q,struct bio *bio);

