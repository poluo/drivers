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
#include <linux/platform_device.h>
#include <linux/kfifo.h>
#include <linux/interrupt.h>
#include "common.h"

#define DEVICE_COUNT	1
#define FIFO_SIZE			256
int sleep_major = 0;
int sleep_first_minor = 0;
module_param(sleep_major, int, 0600);
module_param(sleep_first_minor, int, 0600);

struct sleep_dev
{
    wait_queue_head_t w_queue, r_queue;
    struct fasync_struct *async_queue;
    atomic_t wait_flag;
    struct kfifo in_fifo;
	struct kfifo out_fifo;
    struct mutex read_lock;
	struct mutex write_lock;
    struct cdev cdev;         /* Char device structure      */
};
static struct sleep_dev *my_dev;
static struct tasklet_struct sleep_tasklet;
static struct timer_list sleep_timer;

static void sleep_tasklet_func(unsigned long data)
{
	unsigned char *fifo_buf;
    struct sleep_dev *t_dev = (struct sleep_dev *)data;
	unsigned int to_write, read_left, read, sent;

	PDEBUG("data %p mydev %p\n", t_dev, my_dev);
	
    to_write = kfifo_len(&my_dev->out_fifo);
	read_left = FIFO_SIZE - kfifo_len(&my_dev->in_fifo);

	to_write = to_write > read_left ? read_left : to_write;
	PDEBUG("%s to_write %d\n", __FUNCTION__, to_write);

	if(to_write != 0)
	{
		fifo_buf = kzalloc(to_write, GFP_ATOMIC);// can not sleep
		if(!fifo_buf)
		{
			PDEBUG("kzalloc fifo_buf failed in %s\n",__FUNCTION__);
			goto end;
		}
		else
		{
			if(!mutex_trylock(&my_dev->write_lock))
			{
				PDEBUG("write_lock lock failed in %s\n",__FUNCTION__);
				goto free_buf;
			}

			if(!mutex_trylock(&my_dev->read_lock))
			{
				PDEBUG("read_lock lock failed in %s\n", __FUNCTION__);
				goto unlock_write;
			}

			while(to_write)
			{
				read = kfifo_out(&my_dev->out_fifo, fifo_buf, to_write);
				sent = kfifo_in(&my_dev->in_fifo, fifo_buf, read);

				if(sent != read)
					PDEBUG("read %d sent %d in %s\n", read, sent, __FUNCTION__);

				to_write -= read;
			}
			PDEBUG("in_fifo len %d\n", kfifo_len(&my_dev->in_fifo));
			if(kfifo_len(&my_dev->in_fifo) > 0)
				wake_up(&my_dev->r_queue);
			if(kfifo_len(&my_dev->out_fifo) == 0)
				wake_up(&my_dev->w_queue);
			mutex_unlock(&my_dev->read_lock);
			mutex_unlock(&my_dev->write_lock);
		}
	}
	return;

unlock_write:
	mutex_unlock(&my_dev->write_lock);
free_buf:
	kfree(fifo_buf);
end:
	return;
}

static void sleep_timer_func(unsigned long data)
{
	tasklet_schedule(&sleep_tasklet);
    PDEBUG("%s invoked\n", __FUNCTION__);
	mod_timer(&sleep_timer, jiffies + 1 * HZ);
}

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

ssize_t sleep_write(struct file *filp, const char  __user *buf, size_t len,loff_t *f_pos)
{
    struct sleep_dev *dev  = filp->private_data;
    int ret;
	unsigned int copied;
    PDEBUG(": write function invoked\n");

    if(kfifo_len(&dev->out_fifo) == FIFO_SIZE)
	{
		if(filp->f_flags & O_NONBLOCK)
		{
			return -EAGAIN;
		}
		else
		{
			ret = wait_event_interruptible(dev->w_queue, kfifo_len(&dev->out_fifo) != FIFO_SIZE);
			if(ret == -ERESTARTSYS)
			{
				PDEBUG("wait in_fifo readable interrupted\n");
				return -EINTR;
			}
		}
	}

    if (len > FIFO_SIZE)
    {
        len = FIFO_SIZE;
    }

    if(mutex_lock_interruptible(&dev->write_lock))
	{
		return -EINTR;
	}
    ret = kfifo_from_user(&dev->out_fifo, buf, len, &copied);
    len = ret ? ret : copied;

    wake_up(&dev->r_queue);

    if(dev->async_queue)
        kill_fasync(&dev->async_queue, SIGIO, POLL_IN);

    atomic_inc(&dev->wait_flag);
	mutex_unlock(&dev->write_lock);
    PDEBUG(": write function done, count %zd\n",len);
    return len;

}
ssize_t sleep_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct sleep_dev *dev  = filp->private_data;
    int ret;
	unsigned int len, copied;

    PDEBUG(": read function invoked,may be sleep until someone write\n");

	if(kfifo_len(&dev->in_fifo) == 0)
	{
		if(filp->f_flags & O_NONBLOCK)
		{
			return -EAGAIN;
		}
		else
		{
			ret = wait_event_interruptible(dev->r_queue, kfifo_len(&dev->in_fifo) != 0);
			if(ret == -ERESTARTSYS)
			{
				PDEBUG("wait in_fifo readable interrupted\n");
				return -EINTR;
			}
		}
	}

	len = kfifo_len(&dev->in_fifo);
	count = count > len ? len : count;

	if(mutex_lock_interruptible(&dev->read_lock))
	{
		return -EINTR;
	}	
	
    ret = kfifo_to_user(&dev->in_fifo, buf, count, &copied);
    count = ret ? ret : copied;

    atomic_dec(&dev->wait_flag);
	mutex_unlock(&dev->read_lock);
    PDEBUG(": read function done, count %zd\n", count);
    return count;
}

static unsigned int sleep_poll(struct file *file, poll_table *wait)
{
    struct sleep_dev *dev  = file->private_data;
    unsigned int ret = 0;
    PDEBUG("sleep_poll invoked\n");

    poll_wait(file, &dev->r_queue, wait);
    poll_wait(file, &dev->w_queue, wait);
    if(kfifo_len(&my_dev->in_fifo) != 0)
        ret |= POLLIN;
    if(kfifo_len(&my_dev->out_fifo) != FIFO_SIZE)
        ret |= POLLOUT;
        
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

static struct sleep_dev * init_sleep_dev(void)
{
    int ret;
	struct sleep_dev *tmp_dev = kzalloc(sizeof(struct sleep_dev) * DEVICE_COUNT, GFP_KERNEL);
	if(!tmp_dev)
	{
		PDEBUG(":kmalloc failed on sleep_struct\n");
	    return NULL;
    }
    init_waitqueue_head(&tmp_dev->w_queue);
    init_waitqueue_head(&tmp_dev->r_queue);
    mutex_init(&tmp_dev->read_lock);
    mutex_init(&tmp_dev->write_lock);

    PDEBUG("waitqueue and mutex init success\n");

    ret = kfifo_alloc(&tmp_dev->in_fifo, FIFO_SIZE, GFP_KERNEL);
	if(ret)
	{
		PDEBUG("kfifo_alloc for in_fifo failed\n");
		return NULL;
	}
	ret = kfifo_alloc(&tmp_dev->out_fifo, FIFO_SIZE, GFP_KERNEL);
	if(ret)
	{
		PDEBUG("kfifo_alloc for out_fifo failed\n");
		kfifo_free(&tmp_dev->in_fifo);
		return NULL;
	}
    PDEBUG("kfifo allo success\n");

    atomic_set(&tmp_dev->wait_flag, 0);
    cdev_init(&tmp_dev->cdev, &sleep_fops); 
	tmp_dev->cdev.owner = THIS_MODULE;
	
    init_timer(&sleep_timer);
	sleep_timer.data = 0;
	sleep_timer.function = sleep_timer_func;
	sleep_timer.expires = jiffies + 2*HZ;
	add_timer(&sleep_timer);

    PDEBUG("init timer success\n");
	tasklet_init(&sleep_tasklet, sleep_tasklet_func, (unsigned long)tmp_dev);
	
    return tmp_dev;
}
static int sleep_probe(struct platform_device *pdev)
{
	dev_t dev_id;
	int devno, err;

    PDEBUG("%s invoked\n", __FUNCTION__);
	
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
    else
    {
        PDEBUG("register sleep dev success\n");
    }


    my_dev = init_sleep_dev();
    if(my_dev == NULL)
    {
        unregister_chrdev_region(dev_id, DEVICE_COUNT);
        PDEBUG("init sleep dev failed\n");
    }
    else
    {
        PDEBUG("init sleep dev success\n");
    }
    devno = MKDEV(sleep_major, sleep_first_minor);
    err = cdev_add(&my_dev->cdev, devno, DEVICE_COUNT);
    if (err)
    {
		unregister_chrdev_region(dev_id, DEVICE_COUNT);
        kfree(my_dev);
        my_dev = NULL;
        PDEBUG(": Error %d adding sleep", err);
    }
    PDEBUG("probe done\n");
	return 0;
}
static int sleep_remove(struct platform_device *pdev)
{
	dev_t dev_id = MKDEV(sleep_major, sleep_first_minor);
	cdev_del(&my_dev->cdev);
	kfifo_free(&my_dev->in_fifo);
	kfifo_free(&my_dev->out_fifo);
    kfree(my_dev);
	
	unregister_chrdev_region(dev_id, DEVICE_COUNT);
    del_timer(&sleep_timer);
	tasklet_kill(&sleep_tasklet);
	PDEBUG(": Bye, sleep!\n");
    return 0;
}

static struct platform_driver sleep_drv = {
	.probe		= sleep_probe,
	.remove		= sleep_remove,
	.driver		= {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init m_init(void)
{
	int err = 0;
	PDEBUG(" is loaded\n");

	err = platform_driver_register(&sleep_drv);
	if(err)
	{
		PDEBUG("driver register failed\n");
		return err;
	}

	return err;
}

static void __exit m_exit(void)
{
    platform_driver_unregister(&sleep_drv);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cfn");
MODULE_DESCRIPTION("learning program");
