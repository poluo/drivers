#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#define DEVICE_NAME  "/dev/w25qxx"

void write_device(unsigned int to_write)
{
	int i, seed, fd;
	char *buf = NULL;

	seed = time(NULL);
	srand(seed);

	if(to_write <= 0)
	{
		printf("args 2 must more than zero\n");
		return;
	}
	buf = malloc(to_write);
	if(!buf)
	{
		printf("malloc memory failed ,line %d\n", __LINE__);
		return;
	}
	printf("buf: ");
	for (i = 0; i < to_write; ++i)
	{
		buf[i] = rand() % 255;
		printf(" %d ", buf[i]);
	}
	printf("\n");
	/*first step write data*/
	fd = open(DEVICE_NAME, O_RDWR);
	if(fd < 0)
	{
		printf("open file failed\n");
		free(buf);
		return;
	}

	i = write(fd, buf, to_write);
	if(i != to_write)
	{
		printf("write not complete, to_write %d complete %d\n", to_write, i);
	}
	close(fd);
	free(buf);
}

void read_devcie(unsigned int to_read)
{
	int count, fd, i;
	char *buf = NULL;

	buf = malloc(to_read);
	memset(buf, 0, to_read);
	if(!buf)
	{
		printf("buf malloc failed\n");
		return;
	}

	fd = open(DEVICE_NAME, O_RDWR);
	if(fd < 0)
	{
		printf("file open failed\n");
		free(buf);
		return;
	}

	count = read(fd, buf, to_read);
	if(count != to_read)
	{
		printf("read not complete, to_read %d complete %d\n", to_read, count);
	}
	printf("buf: ");
	for (i = 0; i < count; ++i)
	{
		printf(" %d ", buf[i]);
	}
	printf("\n");

	free(buf);
	close(fd);

}


int main(int argc, char const *argv[])
{

	int i;
	unsigned char role;
	unsigned int count;

	if (argc < 3)
	{
		printf("Usage: %s [r/w] num\n", argv[0]);
		return -1;
	}

	role = *argv[1];
	count = atoi(argv[2]);
	if (count <= 0)
	{
		printf("num to write or read mus more th zero, now %d\n", count);
	}
	if('r' == role || 'R' == role)
	{
		read_devcie(count);
	}
	else if('w' == role || 'W' == role)
	{
		write_device(count);
	}
	else
	{
		printf("Usage: %s [r/w] num\n", argv[0]);
		return -1;
	}
	return 0;
}