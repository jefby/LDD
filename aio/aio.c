#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_LEN 1024

void input_handler(int signo)
{
	char data[MAX_LEN];
	int len;
	
	len = read(STDIN_FILENO,&data,MAX_LEN);
	data[len]=0;
	printf("input available : %s\n",data);
	
}
int main(int argc,char **argv)
{
	int oflags;
	
	//启动信号驱动机制
	signal(SIGIO,input_handler);
	fcntl(STDIN_FILENO,F_SETOWN,getpid());
	oflags = fcntl(STDIN_FILENO,F_GETFL);
	fcntl(STDIN_FILENO,F_SETFL,oflags | FASYNC);

	while(1);
	return 0;
}
