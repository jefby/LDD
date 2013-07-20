#include <stdio.h>
#include <signal.h>
void sig_handler(int signo)
{
	printf("Have caught sign N.O. %d\n",signo);
}
int main(int argc,char **argv)
{
	signal(SIGINT,sig_handler);
	signal(SIGTERM,sig_handler);
	while(1);
	return 0;
}
