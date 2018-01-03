#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/sched.h> 
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/version.h>

#define MODULE_NAME 	"myuart"
#define PDEBUG(format, args...) printk(KERN_WARNING MODULE_NAME "drv: " format, ##args)
#define UART_NUM		2
#define FIFO_SIZE		256

static int myuart_major = 0, myuart_minor = 0;


module_param(myuart_major, int, 0644);
module_param(myuart_minor, int, 0644);


struct myuart_data {
	void *in_buf;
	void *out_buf;
	struct kfifo *in_fifo;
	struct kfifo *out_fifo;
	struct tasklet_struct tlet;
	spinlock_t in_fifo_lock, out_fifo_lock;
	wait_queue_head_t readable, writeable;
	struct mutex read_lock;
	struct mutex write_lock;
	int uart_detected;
};

static struct myuart_data *myuart_data;
static int myuartdrv_open(struct inode *inode, struct file *filp)
{
	PDEBUG("%s invoked\n",__FUNCTION__);
	return 0;
}

static int myuartdrv_release(struct inode *inode, struct file *file)
{
	PDEBUG("%s invoked\n", __FUNCTION__);
	return 0;
}


static ssize_t myuartdrv_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	int ret;
	unsigned int len, copied;
	unsigned char *fifo_buf;
	if(kfifo_len(myuart_data->in_fifo) == 0)
	{
		if(file->f_flags & O_NONBLOCK)
		{
			return -EAGAIN;
		}
		else
		{
			ret = wait_event_interruptible(myuart_data->readable, kfifo_len(myuart_data->in_fifo) != 0);
			if(ret == -ERESTARTSYS)
			{
				PDEBUG("wait in_fifo readable interrupted\n");
				return -EINTR;
			}

		}
	}

	len = kfifo_len(myuart_data->in_fifo);
	len = count > len ? len : count;

	if(mutex_lock_interruptible(&myuart_data->read_lock))
	{
		return -EINTR;
	}	

	#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,32)
	fifo_buf = kzalloc(len, GFP_KERNEL);
	if(!fifo_buf)
	{
		PDEBUG("kzalloc fifo_buf failed\n");
		mutex_unlock(&myuart_data->read_lock);
		return -ENOMEM;
	}
	len = kfifo_get(myuart_data->in_fifo, fifo_buf, len);

	if(copy_to_user(buf, fifo_buf, len))
    {
    	kfree(fifo_buf);
    	mutex_unlock(&myuart_data->read_lock);
        PDEBUG("copy_to_user failed\n");
        return -EFAULT;
    }
    kfree(fifo_buf);
    #else
    ret = kfifo_to_user(myuart_data->in_fifo, fifo_buf, len, &copied);
    len = ret ? ret : copied;
    #endif
	mutex_unlock(&myuart_data->read_lock);
	
	return len;

}

static ssize_t myuartdrv_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	int ret;
	unsigned int len, copied;
	unsigned char *fifo_buf;

	if(kfifo_len(myuart_data->out_fifo) == FIFO_SIZE)
	{
		if(file->f_flags & O_NONBLOCK)
		{
			return -EAGAIN;
		}
		else
		{
			ret = wait_event_interruptible(myuart_data->writeable, kfifo_len(myuart_data->out_fifo) != FIFO_SIZE);
			if(ret == -ERESTARTSYS)
			{
				PDEBUG("wait in_fifo readable interrupted\n");
				return -EINTR;
			}

		}
	}
	
	len = count > FIFO_SIZE ? FIFO_SIZE : count;
	
	if(mutex_lock_interruptible(&myuart_data->write_lock))
	{
		return -EINTR;
	}

	#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,32)
	fifo_buf = kzalloc(len, GFP_KERNEL);
	if(!fifo_buf)
	{
		mutex_unlock(&myuart_data->write_lock)
		PDEBUG("kzalloc fifo_buf failed\n");
		return -ENOMEM;
	}
	if(copy_from_user(fifo_buf, buf, len))
    {
     	kfree(fifo_buf);
     	mutex_unlock(&myuart_data->write_lock);
        PDEBUG("copy_from_user failed\n");
        return -EFAULT;
    }

	len = kfifo_put(myuart_data->out_fifo, fifo_buf, len);
	kfree(fifo_buf);
	#else
	ret = kfifo_from_user(myuart_data->out_fifo, fifo_buf, len, &copied);
	len = ret ? ret : copied;
	#endif
	mutex_unlock(&myuart_data->write_lock);
	
	return len;
}
static unsigned int myuartdrv_poll(struct file *file, poll_table *pt)
{
	unsigned int mask = 0;
	poll_wait(file, &myuart_data->readable, pt);
	poll_wait(file, &myuart_data->writeable, pt);

	if (kfifo_len(myuart_data->in_fifo) != 0)
		mask |= POLLIN | POLLRDNORM;
	if (kfifo_len(myuart_data->out_fifo) != FIFO_SIZE)
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}
static const struct file_operations myuartdrv_fops = {
	.owner	= THIS_MODULE,
	.open	= myuartdrv_open,
	.release = myuartdrv_release,
	.read	= myuartdrv_read,
	.write	= myuartdrv_write,
	.poll	= myuartdrv_poll,
};

static struct dentry *debugfs;

static void myuart_tasklet_func(unsigned long data)
{
	unsigned char *fifo_buf;

	unsigned int to_write, read_left, read, sent;
	
	to_write = kfifo_len(myuart_data->out_fifo);
	read_left = FIFO_SIZE - kfifo_len(myuart_data->in_fifo);

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
			if(mutex_lock_interruptible(&myuart_data->write_lock))
			{
				PDEBUG("write_lock lock failed in %s\n",__FUNCTION__);
				goto free_buf;
			}

			if(mutex_lock_interruptible(&myuart_data->read_lock))
			{
				PDEBUG("read_lock lock failed in %s\n", __FUNCTION__);
				goto unlock_write;
			}

			while(to_write)
			{
				#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,32)
				read = kfifo_get(myuart_data->out_fifo, fifo_buf, to_write);
				sent = kfifo_put(myuart_data->in_fifo, fifo_buf, read);
				#else
				read = kfifo_out_locked(myuart_data->out_fifo, fifo_buf, to_write, &myuart_data->out_fifo_lock);
				sent = kfifo_in_locked(myuart_data->in_fifo, fifo_buf, read, &myuart_data->in_fifo_lock);
				#endif

				if(sent != read)
					PDEBUG("read %d sent %d in %s\n", read, sent, __FUNCTION__);

				to_write -= read;
			}
			PDEBUG("in_fifo len %d\n", kfifo_len(myuart_data->in_fifo));
			if(kfifo_len(myuart_data->in_fifo) > 0)
				wake_up(&myuart_data->readable);
			if(kfifo_len(myuart_data->out_fifo) == 0)
				wake_up(&myuart_data->writeable);
			mutex_unlock(&myuart_data->read_lock);
			mutex_unlock(&myuart_data->write_lock);
		}
	}
	return;

unlock_write:
	mutex_unlock(&myuart_data->write_lock);

free_buf:
	kfree(fifo_buf);
end:
	return;
}

static struct tasklet_struct myuart_tasklet;

static struct timer_list myuart_timer;

static void myuart_timer_func(unsigned long data)
{
	tasklet_schedule(&myuart_tasklet);
	mod_timer(&myuart_timer, jiffies + 1 * HZ);
}

//static DEFINE_TIMER(myuart_timer, myuart_timer_func, 0, 0);

struct myuart_data *myuart_data_init(void)
{
	struct myuart_data *tmp_data;
	int ret;
	tmp_data = kzalloc(sizeof(*tmp_data), GFP_KERNEL);
	if(!tmp_data)
		return NULL;

	init_waitqueue_head(&tmp_data->readable);
	init_waitqueue_head(&tmp_data->writeable);
	mutex_init(&tmp_data->read_lock);
	mutex_init(&tmp_data->write_lock);
	spin_lock_init(&tmp_data->in_fifo_lock);
	spin_lock_init(&tmp_data->out_fifo_lock);
	#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,32)
	tmp_data->in_fifo = kfifo_alloc(FIFO_SIZE, GFP_KERNEL, &tmp_data->in_fifo_lock);
	if(IS_ERR(tmp_data->in_fifo))
	{
		PDEBUG("kfifo_alloc for in_fifo failed\n");
		goto out1;
	}
	tmp_data->out_fifo = kfifo_alloc(FIFO_SIZE, GFP_KERNEL, &tmp_data->out_fifo_lock);
	if(IS_ERR(tmp_data->out_fifo))
	{
		PDEBUG("kfifo_alloc for out_fifo failed\n");
		goto out2;
	}
	#else 
	ret = kfifo_alloc(tmp_data->in_fifo, FIFO_SIZE, GFP_KERNEL);
	if(ret)
	{
		PDEBUG("kfifo_alloc for in_fifo failed\n");
		goto out1;
	}
	ret = kfifo_alloc(tmp_data->out_fifo, FIFO_SIZE, GFP_KERNEL);
	if(ret)
	{
		PDEBUG("kfifo_alloc for out_fifo failed\n");
		kfifo_free(myuart_data->in_fifo);
		goto out1;
	}
	#endif
	init_timer(&myuart_timer);
	myuart_timer.data = 0;
	myuart_timer.function = myuart_timer_func;
	myuart_timer.expires = jiffies + 2*HZ;
	add_timer(&myuart_timer);

	tasklet_init(&myuart_tasklet, myuart_tasklet_func, 0);

	return tmp_data;
out2:
	kfifo_free(tmp_data->in_fifo);
out1:
	kfree(tmp_data);
	return NULL;
}
int myuart_probe(struct platform_device *platdev)
{
	int err = 0;
	dev_t devno;

	PDEBUG("%s \n", __FUNCTION__);
	if(myuart_major)
	{
		devno = MKDEV(myuart_major, myuart_minor);
		err = register_chrdev_region(devno, 1, MODULE_NAME);
	}
	else
	{
		#if 0
		err = alloc_chrdev_region(&devno, 0, 1, MODULE_NAME);
		if(!err)
		{
			myuart_major = MAJOR(devno);
			myuart_minor = MINOR(devno);
			PDEBUG("myuart major %d minor %d\n", myuart_major, myuart_minor);
		}
		#endif
		
	}
	debugfs = debugfs_create_file(MODULE_NAME, S_IRUGO, NULL, NULL, &myuartdrv_fops);

	if(IS_ERR(debugfs))
	{
		err = PTR_ERR(debugfs);
		PDEBUG("debugfs_create_file failed\n");
		goto out;

	}
	PDEBUG("debugfs create success\n");
	myuart_data = myuart_data_init();
	if(!myuart_data)
	{
		PDEBUG("myuart_data init failed\n");
		err = -1;
		goto out;
	}

	PDEBUG("Just print something\n");

out:
	return err;
}
int myuart_remove(struct platform_device *platdev)
{
	int err = 0;

	kfifo_free(myuart_data->in_fifo);
	kfifo_free(myuart_data->out_fifo);
	kfree(myuart_data);
	
	return err;
}


static struct platform_driver myuart_drv = {
	.probe		= myuart_probe,
	.remove		= myuart_remove,
	.driver		= {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
	},
};


static int __init myuart_init(void)
{
	int err = 0;
	PDEBUG(" is loaded\n");

	err = platform_driver_register(&myuart_drv);
	if(err)
	{
		PDEBUG("driver register failed\n");
		return err;
	}

	return err;
}

static void __exit myuart_exit(void)
{
	PDEBUG(" unloaded\n");
	debugfs_remove(debugfs);
	del_timer(&myuart_timer);
	tasklet_kill(&myuart_tasklet);
	platform_driver_unregister(&myuart_drv);

}


module_init(myuart_init);
module_exit(myuart_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cfn");
MODULE_DESCRIPTION("A simple platform module");
