#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#define DEV_FILE		"/dev/ioctl"


int main(int argc, char *argv[])
{

	int fd = open(DEV_FILE, O_RDWR);
	unsigned char buf[10] = {0};
	int ret = 0, i = 0;
	unsigned int to_read = 0, to_write = 0;

	to_write = 8;
	buf[0] = 0x7;
	buf[1] = 0x5A;
	buf[5] = 0xff;
	buf[6] = 0x55;
	buf[7] = 0xAA;
	ret = write(fd, buf, to_write);
	if(ret != to_write)
	{
		printf("write 0x00 error,ret = %d , to_write = %d\n", ret, to_write);
	}
	to_read = to_write;
	memset(buf, 0, sizeof(buf));
	ret = read(fd, buf, to_read);
	if(ret != to_read)
	{
		printf("read error ,err = %d, to_read %d\n", ret, to_read);
	}
	printf("buf: \n");
	for(i = 0 ; i< to_read; i++)
	{
		printf("%d ",buf[i]);
	}
	printf("\n");
	close(fd);
	return 0;	
}
