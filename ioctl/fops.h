#ifndef _FOPS_H
#define _FOPS_H

#include <linux/ioctl.h>
#include <linux/wait.h>
#define IOCTL_QUANTUM 	2000
#define IOCTL_QSET 		10
#define IOCTL_IOC_MAGIC  'k'

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically

 */
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


#define IOCTL_IOC_MAXNR 17

extern int ioctl_open(struct inode *inode, struct file *filp);

extern int ioctl_release(struct inode *inode, struct file *filp);

extern long ioctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

extern ssize_t ioctl_write(struct file *filp, const char  __user *buf, size_t count,loff_t *f_pos);

extern ssize_t ioctl_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);

#define IOCTL_BUF_LEN	128
struct ioctl_dev
{
    int quantum;              /* the current quantum size */
    int qset;                 /* the current array size */
	struct mutex ioctl_mutex; 
	unsigned char mem[IOCTL_BUF_LEN];
	unsigned int current_len;
	wait_queue_head_t r_wait;
	wait_queue_head_t w_wait;
    struct cdev cdev;     	  /* Char device structure      */
};
#endif
