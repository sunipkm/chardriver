#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

volatile sig_atomic_t done = 0;
void sighandler(int sig)
{
	done = 1;
}

int main()
{
	signal(SIGINT, &sighandler);
	int fd = open("/dev/charDev", O_RDONLY);
	int ctr = 0;
	while (!done)
	{
		char buf[1024];
		int rdsz = read(fd, buf, 1024);
		if (rdsz > 0)
		{
			printf("%d: ", ctr++);
			for (int i = 0; i < rdsz; i++)
				printf("%c", buf[i]);
			printf("\n");
		}
		else
		{
			printf("%d: Received nothing\n", ctr);
		}
	}
	close(fd);
	return 0;
}
