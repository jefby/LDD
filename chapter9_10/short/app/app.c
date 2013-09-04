


#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

int myatoi(char *buf)
{
	int   x=0;
	int i=0;
	for(i=0;i<2;++i)
	{
	if(buf[i]>='0'&&buf[i]<='9')
		x+=buf[i]-'0';
	else if(buf[i]>='a'&&buf[i] <='f')
		x+=buf[i]-'a'+10;
	if(i==0)
		x=x*16;
	}
	return x; 
		
		
}
int main(int argc,char **argv)
{
	char buf = (char)myatoi(argv[1]);
	printf("%d",(unsigned char)buf);
	int fd = open("/dev/short",O_RDWR);

	if(fd > 0)
	{
		write(fd,&buf,1);
		close(fd);
		return 0;
	}
	return -1;
}
