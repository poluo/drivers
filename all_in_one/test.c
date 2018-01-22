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
#include <sys/ioctl.h>

#define DEVICE_NAME  "/dev/all"
#define IOCTL_IOC_MAGIC		'S'
#define IOCTL_TEST_SLEEP		_IOW(IOCTL_IOC_MAGIC, 0, int)
#define IOCTL_TEST_TIME			_IO(IOCTL_IOC_MAGIC, 1)
#define IOCTL_TEST_TIMER		_IOW(IOCTL_IOC_MAGIC, 2, int)
#define IOCTL_HOWMANY			_IOR(IOCTL_IOC_MAGIC, 3, int)
#define IOCTL_MESSAGE			_IOWR(IOCTL_IOC_MAGIC, 4, int)
#define IOCTL_MAX_NR			4
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
#define IOCTL_IOC_MAGIC		'S'
#define IOCTL_TEST_SLEEP		_IOW(IOCTL_IOC_MAGIC, 0, int)
#define IOCTL_TEST_TIME			_IO(IOCTL_IOC_MAGIC, 1)
#define IOCTL_TEST_TIMER		_IOW(IOCTL_IOC_MAGIC, 2, int)
#define IOCTL_HOWMANY			_IOR(IOCTL_IOC_MAGIC, 3, int)
#define IOCTL_MESSAGE			_IOWR(IOCTL_IOC_MAGIC, 4, int)
#define IOCTL_MAX_NR			4
void ioctl_deivce(unsigned int type)
{
	int cmd, fd, i;

	fd = open(DEVICE_NAME, O_RDWR);
	if(fd < 0)
	{
		printf("file open failed\n");
		return;
	}
	switch(type)
	{
		case 0:
			cmd = 0;
			ioctl(fd, IOCTL_TEST_SLEEP, &cmd);
			cmd = 3;
			ioctl(fd, IOCTL_TEST_SLEEP, &cmd);
			cmd = 4;
			ioctl(fd, IOCTL_TEST_SLEEP, &cmd);
			
			break;
		case 1:
			ioctl(fd, IOCTL_TEST_TIME, &cmd);
			break;
		case 2:
			cmd = 0x55;
			ioctl(fd, IOCTL_TEST_TIMER, &cmd);
			break;
		case 3:
			ioctl(fd, IOCTL_HOWMANY, &cmd);
			printf("how many %d\n", cmd);
			break;
		case 4:
			cmd = 103726;
			ioctl(fd, IOCTL_MESSAGE, &cmd);
			printf("message %d\n", cmd);
	}
	
	close(fd);

}
void poll_device(unsigned int count)
{
	struct pollfd *fds;
	int fd, ret;

	fds = malloc(sizeof(struct pollfd));
	if (NULL == fds)
	{
		printf("malloc failed\n");
		return;
	}

	fd = open(DEVICE_NAME, O_RDWR);
	if(fd < 0)
	{
		printf("open fiel failed\n");
		return;
	}

	fds->fd = fd;
	fds->events = POLLIN | POLLOUT;
	ret = poll(fds, 1, -1);
	if(ret == -1)
	{
		printf("something wrong happened\n");
		return;
	}
	if(fds->revents & POLLIN)
	{
		close(fd);
		printf("ready to read\n");
		read_devcie(count);
	}
	else if(fds->revents & POLLOUT)
	{
		close(fd);
		printf("ready to write\n");
		write_device(count);
	}
}

void select_device(unsigned int count)
{
	struct timeval timeout;
	struct timeval *timeput_ptr = NULL;
	int ready, nfds, fd;
	fd_set readfs, writefs;

	timeout.tv_sec = 3;
	timeout.tv_usec = 0;

	// timeput_ptr = &timeout;
	fd = open(DEVICE_NAME, O_RDWR);
	if (fd < 0)
	{
		printf("open file failed\n");
		return;
	}

	nfds = 0;
	FD_ZERO(&readfs);
	FD_ZERO(&writefs);

	FD_SET(fd, &readfs);
	FD_SET(fd, &writefs);
	
	nfds = fd + 1;

	ready = select(nfds, &readfs, &writefs, NULL, timeput_ptr);
	if(ready == -1)
	{
		printf("some thing wrong happened\n");
		return;
	}
	else if(ready == 0)
	{
		printf("select timeout\n");
		return;
	}
	
	printf("ready for %s %s\n", FD_ISSET(fd, &readfs) ? "r":"", FD_ISSET(fd, &writefs) ? "w":"");
	if(FD_ISSET(fd, &readfs))
	{
		printf("ready to read\n");
		close(fd);
		read_devcie(count);
	}
	else if(FD_ISSET(fd, &writefs))
	{
		close(fd);
		printf("ready to write\n");
		write_device(count);
	}

}
static int gotdata = 0;
void sig_handler(int arg)
{
	if(arg == SIGIO)
		gotdata++;
}

void async_device(unsigned int count)
{
	struct sigaction action;
	int got, i, fd;
	char *buf = NULL;

	buf = malloc(count);
	memset(buf, 0, count);
	if(!buf)
	{
		printf("buf malloc failed\n");
		return;
	}

	memset(&action, 0 ,sizeof(struct sigaction));
	action.sa_handler = sig_handler;
	action.sa_flags = 0;

	sigaction(SIGIO, &action, NULL);

	fd = open(DEVICE_NAME, O_RDWR);
	if(fd < 0)
	{
		printf("open file failed\n");
		return;
	}

	fcntl(fd, F_SETOWN, getpid());
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | FASYNC);

	while(1)
	{
		sleep(1);
		if(!gotdata)
			continue;
		gotdata = 0;
		got = read(fd, buf, count);
		if(got != count)
		{
			printf("read not complete, got %d should  read %d\n", got, count);
		}
		printf("buf: ");
		for (i = 0; i < got; ++i)
		{
			printf("%d ", buf[i]);
		}
		printf("\n");
		if (got > 0)
		{
			break;
		}

	}
}
int main(int argc, char const *argv[])
{

	int i, seed, fd;
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
	else if('p' == role || 'p' == role)
	{
		poll_device(count);
	}
	else if('s' == role || 'S' == role)
	{
		select_device(count);
	}
	else if('a' == role || 'A' == role)
	{
		async_device(count);
	}
	else if('O' == role || 'o' == role)
	{
		ioctl_deivce(count);
	}
	else
	{
		printf("Usage: %s [r/w] num\n", argv[0]);
		return -1;
	}
	return 0;
}