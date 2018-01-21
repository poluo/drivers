#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <asm/hardirq.h>
#include <linux/ioctl.h>

#include "common.h"

#define DEVICE_COUNT	1
#define BUF_LEN			256
static struct sleep_dev *my_dev;
int sleep_major = 0;
int sleep_first_minor = 0;
module_param(sleep_major, int, 0600);
module_param(sleep_first_minor, int, 0600);

struct sleep_dev
{
    wait_queue_head_t w_queue, r_queue;
    struct semaphore sem;
    struct fasync_struct *async_queue;
    atomic_t wait_flag;
    unsigned char w_pos, r_pos;
    unsigned char mem[BUF_LEN];
    struct cdev cdev;         /* Char device structure      */
};

int sleep_open(struct inode *inode, struct file *filp)
{
    struct sleep_dev *dev;
    unsigned long j;

    j = jiffies;
    dev = container_of(inode->i_cdev,struct sleep_dev,cdev);
    filp->private_data = dev;
    PDEBUG(": sleep_open invoked, jiffies %ld\n",j);
    PDEBUG(": in interrupt or not %s", in_interrupt() ? "TRUE" : "FALSE");
    return 0;
}

ssize_t sleep_write(struct file *filp, const char  __user *buf, size_t count,loff_t *f_pos)
{
    struct sleep_dev *dev  = filp->private_data;
    PDEBUG(": write function invoked\n");

  
    if(down_interruptible(&dev->sem))
    {
        PDEBUG(": down sem interrupted by signal\n");
        return -EINTR;
    }
    if (count > BUF_LEN)
    {
        count = BUF_LEN;
    }
    PDEBUG("write count %zd\n",count);
    while(dev->w_pos)
    {
        DEFINE_WAIT(my_wait);
        up(&dev->sem);
        if(filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        prepare_to_wait(&dev->w_queue, &my_wait, TASK_INTERRUPTIBLE);
        if(dev->w_pos)
            schedule();
        finish_wait(&dev->w_queue, &my_wait);
        if(signal_pending(current))
        {
            return -ERESTARTSYS;
        }
        if(down_interruptible(&dev->sem))
        {
            PDEBUG(": down sem interrupted by signal\n");
            return -ERESTARTSYS;
        }
    }

    PDEBUG(": Now you can write, count %zd, w_pos %d\n", count, dev->w_pos);
    if(copy_from_user(dev->mem, buf, count))
    {
        up(&dev->sem);
        PDEBUG(": copy_from_user failed\n");
        return -EFAULT;
    }
    atomic_inc(&dev->wait_flag);
    dev->w_pos = count;
    dev->r_pos = 0;
    PDEBUG("w_pos %d r_pos %d\n", dev->w_pos, dev->r_pos);
    wake_up(&dev->r_queue);
    if(dev->async_queue)
        kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
    up(&dev->sem);
    PDEBUG(": write function done, count %zd\n",count);
    return count;

}
ssize_t sleep_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct sleep_dev *dev  = filp->private_data;
    PDEBUG(": read function invoked,may be sleep until someone write\n");
    if(down_interruptible(&dev->sem))
    {
        PDEBUG(": down sem interrupted by signal\n");
        return -EINTR;
    }
    
    while(!dev->w_pos)
    {
        DEFINE_WAIT(my_wait);
        up(&dev->sem);
        if(filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        prepare_to_wait(&dev->r_queue, &my_wait, TASK_INTERRUPTIBLE);
        if(!dev->w_pos)
            schedule();
        finish_wait(&dev->r_queue, &my_wait);
        if(signal_pending(current))
        {
            return -ERESTARTSYS;
        }
        if(down_interruptible(&dev->sem))
        {
            PDEBUG(": down sem interrupted by signal\n");
            return -ERESTARTSYS;
        }
    }

    if(count > dev->w_pos)
        count = dev->w_pos;
    PDEBUG("read count %zd\n", count);

    atomic_dec(&dev->wait_flag);
    if(copy_to_user(buf, dev->mem + dev->r_pos, count))
    {
        PDEBUG("copy_to_user failed\n");
    }
    dev->w_pos -= count;
    dev->r_pos += count;
    PDEBUG("w_pos %d r_pos %d\n", dev->w_pos, dev->r_pos);
    up(&dev->sem);
    wake_up(&dev->w_queue);
    PDEBUG(": read function done, count %zd\n", count);
    return count;
}

static unsigned int sleep_poll(struct file *file, poll_table *wait)
{
    struct sleep_dev *dev  = file->private_data;
    unsigned int ret = 0;
    PDEBUG("sleep_poll invoked\n");
    if(down_interruptible(&dev->sem))
    {
        PDEBUG(": down sem interrupted by signal\n");
        return -EINTR;
    }
    poll_wait(file, &dev->r_queue, wait);
    poll_wait(file, &dev->w_queue, wait);
    if(dev->w_pos)
        ret |= POLLIN;
    else
        ret |= POLLOUT;
    up(&dev->sem);
    return ret;
}

#define IOCTL_IOC_MAGIC		'S'
#define IOCTL_TEST_SLEEP		_IOW(IOCTL_IOC_MAGIC, 0, int)
#define IOCTL_TEST_TIME			_IO(IOCTL_IOC_MAGIC, 1)
#define IOCTL_TEST_TIMER		_IOW(IOCTL_IOC_MAGIC, 2, int)
#define IOCTL_HOWMANY			_IOR(IOCTL_IOC_MAGIC, 3, int)
#define IOCTL_MESSAGE			_IOWR(IOCTL_IOC_MAGIC, 4, int)
#define IOCTL_MAX_NR			4

static long sleep_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	#define IOCTL_BUF_LEN 128
	static unsigned char ioctl_buf[128];
	static unsigned long pos = 0;
	//struct sleep_dev *dev  = file->private_data;
    unsigned int ret = 0;
	unsigned long ioarg;
	
	if(_IOC_TYPE(cmd) != IOCTL_IOC_MAGIC)
		return -EINVAL;
	if(_IOC_NR(cmd) > IOCTL_MAX_NR)
		return -EINVAL;

	if (_IOC_DIR(cmd) & _IOC_READ)
		ret = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
        ret =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (ret) 
		return -EFAULT;

	switch(cmd)
	{
		case IOCTL_TEST_SLEEP:
			ret = __get_user(ioarg, (int *)arg);
			PDEBUG("IOCTL_TEST_SELLP arg %lu\n", ioarg);
			test_sleep_method(ioarg);
			break;

		case IOCTL_TEST_TIME:
			print_time();
			break;
		
		case IOCTL_TEST_TIMER:
			ret = __get_user(ioarg, (int *)arg);
			if(ret)
				PDEBUG("IOCTL_TEST_TIMER get user fialed, ret %#x\n", ret);
			PDEBUG("IOCTL_TEST_SELLP arg %lu\n", ioarg);
			test_timer(ioarg);
			break;

		case IOCTL_HOWMANY:
			ioarg = IOCTL_BUF_LEN - pos;
			PDEBUG("HOWMANY left %lu\n", ioarg);
			ret = __put_user(ioarg, (int *)arg);
			if(ret)
				PDEBUG("IOCTL_HOWMANY put user fialed, ret %#x\n", ret);
			break;
		case IOCTL_MESSAGE:
			ret = __get_user(ioarg, (int *)arg);
			if(ret)
				PDEBUG("IOCTL_MESSAGE get user fialed, ret %#x\n", ret);
			memcpy((ioctl_buf + pos), &ioarg, sizeof(ioarg));
			pos += sizeof(ioarg);
			ioarg = IOCTL_BUF_LEN - pos;
			PDEBUG("HOWMANY left %lu\n", ioarg);
			ret = __put_user(ioarg, (int *)arg);
			if(ret)
				PDEBUG("IOCTL_MESSAGE put user fialed, ret %#x\n", ret);
			break;
        default:
            return -EINVAL;
	}
    return ret;
	#undef IOCTL_BUF_LEN
}
static int sleep_fasync(int fd, struct file *file, int mode)
{
    struct sleep_dev *dev = file->private_data;
    return fasync_helper(fd, file, mode, &dev->async_queue);
}    

static struct file_operations sleep_fops={
	.owner = THIS_MODULE,
	.open = sleep_open,
	.write = sleep_write,
	.read = sleep_read,
    .poll = sleep_poll,
	.unlocked_ioctl = sleep_ioctl,
    .fasync = sleep_fasync,
};
static void  init_sleep_dev(struct sleep_dev *my_dev)
{
    init_waitqueue_head(&my_dev->w_queue);
    init_waitqueue_head(&my_dev->r_queue);
    sema_init(&my_dev->sem, 1);
    my_dev->w_pos = 0;
    my_dev->r_pos = 0;
    atomic_set(&my_dev->wait_flag, 0);
    cdev_init(&my_dev->cdev, &sleep_fops); 
	my_dev->cdev.owner = THIS_MODULE;
}
static int __init m_init(void)
{
	dev_t dev_id;
	int devno, err;

	if(sleep_major)
	{
		err = register_chrdev_region(sleep_major, DEVICE_COUNT, MODULE_NAME);
	}
	else 
	{
		err = alloc_chrdev_region(&dev_id, sleep_first_minor, DEVICE_COUNT, MODULE_NAME);
    	sleep_major = MAJOR(dev_id);
		sleep_first_minor = MINOR(dev_id);
	}
   
    if(err)
    {
    	PDEBUG(": register sleep dev failed\n");
    	return err;
    }

	my_dev = kzalloc(sizeof(struct sleep_dev) * DEVICE_COUNT, GFP_KERNEL);
	if(!my_dev)
	{
		PDEBUG(":kmalloc failed on sleep_struct\n");
		unregister_chrdev_region(dev_id, DEVICE_COUNT);
	    return -ENOMEM;
    }
    init_sleep_dev(my_dev);
    devno = MKDEV(sleep_major, sleep_first_minor);
    err = cdev_add(&my_dev->cdev, devno, DEVICE_COUNT);
    if (err)
    {
		unregister_chrdev_region(dev_id, DEVICE_COUNT);
        kfree(my_dev);
        my_dev = NULL;
        PDEBUG(": Error %d adding sleep", err);
    }
    PDEBUG("m_init done\n");

	return 0;
}

static void __exit m_exit(void)
{
	dev_t dev_id = MKDEV(sleep_major, sleep_first_minor);
	cdev_del(&my_dev->cdev);
    kfree(my_dev);
	unregister_chrdev_region(dev_id, DEVICE_COUNT);
	PDEBUG(": Bye, sleep!\n");
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cfn");
MODULE_DESCRIPTION("learning program");
