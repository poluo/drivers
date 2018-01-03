#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#define DEV_FILE		"/dev/ioctl"

#define IOCTL_IOC_MAGIC		'k'


#define IOCTL_IOCRESET    _IO(IOCTL_IOC_MAGIC, 0)

#define IOCTL_IOCSQUANTUM _IOW(IOCTL_IOC_MAGIC,  1, int)

#define IOCTL_IOCSQSET    _IOW(IOCTL_IOC_MAGIC,  2, int)

#define IOCTL_IOCTQUANTUM _IO(IOCTL_IOC_MAGIC,   3)

#define IOCTL_IOCTQSET    _IO(IOCTL_IOC_MAGIC,   4)

#define IOCTL_IOCGQUANTUM _IOR(IOCTL_IOC_MAGIC,  5, int)

#define IOCTL_IOCGQSET    _IOR(IOCTL_IOC_MAGIC,  6, int)

#define IOCTL_IOCQQUANTUM _IO(IOCTL_IOC_MAGIC,   7)

#define IOCTL_IOCQQSET    _IO(IOCTL_IOC_MAGIC,   8)

#define IOCTL_IOCXQUANTUM _IOWR(IOCTL_IOC_MAGIC, 9, int)

#define IOCTL_IOCXQSET    _IOWR(IOCTL_IOC_MAGIC,10, int)

#define IOCTL_IOCHQUANTUM _IO(IOCTL_IOC_MAGIC,  11)

#define IOCTL_IOCHQSET    _IO(IOCTL_IOC_MAGIC,  12)

#define LEDCTL_ON		  _IO(IOCTL_IOC_MAGIC,  13)

#define LEDCTL_OFF		  _IO(IOCTL_IOC_MAGIC,  14)

#define LEDCTL_GET		  _IOR(IOCTL_IOC_MAGIC,  15, unsigned int)

#define LEDCTL_SET		  _IOW(IOCTL_IOC_MAGIC,  16, unsigned int)

int main(int argc, char *argv[])
{

	int fd = open(DEV_FILE, O_RDWR);
	unsigned char buf[10] = {0};
	unsigned int ret = 0, i = 0;

	ret = ioctl(fd, LEDCTL_ON);
	printf("ioctl LEDCTL_ON return vale %d\n",ret);
	
	ret = ioctl(fd, LEDCTL_OFF);
	printf("ioctl  LEDCTL_OFF return vale %d\n",ret);
	
	ret = ioctl(fd, LEDCTL_GET, &buf[0]);
	if(ret)
	{
		printf("ioctl LEDCTL_GET ret %#x\n",ret);
	}
	printf("LEDCTL_GET %#x\n",buf[0]);

	buf[0] = 0x7;
	ret = ioctl(fd, LEDCTL_SET, &buf[0]);
	if(ret)
	{
		printf("ioctl LEDCTL_SET ret %#x\n",ret);
	}

	memset(buf, 0, sizeof(buf));
	ret = read(fd, buf, sizeof(buf));
	printf("ret %d read %d\n", ret, sizeof(buf));
	printf("buf ");
	for (i = 0; i < sizeof(buf); ++i)
	{
		printf("%d ",buf[i]);
	}
	printf("\n");

	memset(buf, 0, sizeof(buf));
	buf[0] = 0x07;
	buf[1] = 0x5A;
	buf[2] = 0xA5;
	ret = write(fd, buf, 3);
	printf("write %d chars\n", ret);

	memset(buf, 0, sizeof(buf));
	ret = read(fd, buf, 3);
	printf("buf ret %d\n", ret);
	for (i = 0; i < sizeof(buf); ++i)
	{
		printf("%#x ",buf[i]);
	}
	printf("\n");
	close(fd);
	return 0;	
}
