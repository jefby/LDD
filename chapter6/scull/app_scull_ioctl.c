#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h> //open
#include <unistd.h> //close
//#include "scull.h"
#define SCULL_IOC_MAGIC 'K'
#define SCULL_IOCGQUANTUM _IOR(SCULL_IOC_MAGIC,5,int)

int main()
{
	int fd,quantum;
	fd = open("/dev/scull0",O_RDWR);
	if(fd < 2){
		printf("open file error!\n");
		return -1;
	}
	ioctl(fd,SCULL_IOCGQUANTUM,&quantum);
	printf("quantum is %d\n",quantum);
	close(fd);
	
	
	return 0;
}
