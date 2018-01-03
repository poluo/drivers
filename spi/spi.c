#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#define MODULE_NAME 	"PT100drv"
#define DEIVCE_NUM		1
#define PDEBUG(format, args...) printk(KERN_WARNING MODULE_NAME ": " format, ##args)
#define FIFO_SIZE		256

static const struct i2c_device_id pt100_id[]= 
{
	{"PT100", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, pt100_id);

static int pt100_major = 0, pt100_minor = 0;

module_param(pt100_major, int, 0644);
module_param(pt100_minor, int, 0644);
static unsigned int write_max = 64;
static bool use_smbus = 0;
static unsigned int write_timeout = 10;
#define loop_until_timeout(tout, op_time)				\
	for (tout = jiffies + msecs_to_jiffies(write_timeout), op_time = 0; \
	     op_time ? time_before(op_time, tout) : true;		\
	     usleep_range(1000, 1500), op_time = jiffies)
struct pt100_data
{
	dev_t devno;
	struct dentry *debugfs;
	struct cdev chr_cdev;
	struct mutex mutex_lock;
	wait_queue_head_t wq;
	spinlock_t spin_lock;
	struct kfifo *myfifo;
	struct kfifo fifo;
	struct i2c_client *client;
};

static struct timer_list myuart_timer;
static struct tasklet_struct myuart_tasklet;

static int pt100_i2c_read(struct i2c_client *client, unsigned char *buf, size_t len)
{
	char reg = 0x00;
	int ret = 0;
	unsigned char count = 50;
	unsigned char data[5];
	struct i2c_msg msg[2];

	msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].len = 1;
    msg[0].buf = &reg; //指定从地址为0x00的寄存器开始读

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD; //读
    msg[1].len = 5; //共接收5字节数据
    msg[1].buf = data;

    while(count--)
    {
    	ret = i2c_transfer(client->adapter, msg, 2);
    	if(ret && ret!= -EAGAIN)
    		break;
    	if(ret > 0)
    		break;
    	msleep(2);
    	memset(data, 0, sizeof(data));
    }
	
	PDEBUG("data %d %d %d %d %d\n",data[0], data[1], data[2], data[3], data[4]);
	
	PDEBUG("i2c smbus ret %#x %d\n",ret, ret);

	return 0;
}

static void myuart_tasklet_func(unsigned long arg)
{
	struct pt100_data *data = (struct pt100_data *)arg;
	unsigned char buf[7] = {0};
	int i;

    if(!pt100_i2c_read(data->client, buf, 5))
    {
        kfifo_in_locked(data->myfifo, buf, 5, &data->spin_lock);
        PDEBUG("buf: ");
       	for (i = 0; i < 5; ++i)
       	{
       		PDEBUG("%#x ",buf[i]);
       	}
       	PDEBUG("\n");
    }
    else
    {
        PDEBUG("read i2c failed\n");
    }
 	
	PDEBUG("do something else in tasklet\n");
}
static void myuart_timer_func(unsigned long data)
{
	tasklet_schedule(&myuart_tasklet);
	PDEBUG("%s INVOKED\n", __FUNCTION__);
	mod_timer(&myuart_timer, jiffies + 5 * HZ);
}

static int pt100_open(struct inode *inode, struct file *file)
{
	struct pt100_data *data = container_of(inode->i_cdev, struct pt100_data, chr_cdev);
	
	if(!data)
		return -ENODEV;

	file->private_data = data;
	
	return 0;
}

static ssize_t pt100_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct pt100_data *data = file->private_data;
	unsigned int len;
	int ret, i;
	unsigned char *fifo_buf;

	if(kfifo_len(data->myfifo) == 0)
	{
		if(file->f_flags & O_NONBLOCK)
		{
			return -EAGAIN;
		}
		else
		{
			ret = wait_event_interruptible(data->wq, kfifo_len(data->myfifo) != 0);
			if(ret == -ERESTARTSYS)
			{
				PDEBUG("wait in_fifo readable interrupted\n");
				return -EINTR;
			}

		}
	}

	len = kfifo_len(data->myfifo);
	len = count > len ? len : count;
	fifo_buf = kzalloc(len, GFP_KERNEL);
	if(!fifo_buf)
	{
		PDEBUG("kzalloc fifo_buf failed\n");
		return -ENOMEM;
	}
	if(mutex_lock_interruptible(&data->mutex_lock))
	{
		kfree(fifo_buf);
		return -EINTR;
	}	

	len = kfifo_out_locked(data->myfifo, fifo_buf, len, &data->spin_lock);
	if(copy_to_user(buf, fifo_buf, len))
    {
    	kfree(fifo_buf);
        PDEBUG("copy_to_user failed\n");
        return -EFAULT;
    }

	mutex_unlock(&data->mutex_lock);
	kfree(fifo_buf);
	return len;
}

static unsigned int pt100_poll(struct file *file, poll_table *pt)
{
	unsigned int mask = 0;
	struct pt100_data *data = file->private_data;
	poll_wait(file, &data->wq, pt);

	if(kfifo_len(data->myfifo) > 0)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}	

static int pt100_release(struct inode *inode, struct file *file)
{
	PDEBUG("%s INVOKED\n", __FUNCTION__);
	return 0;
}

static const struct file_operations pt100_fops =
{
	.owner = THIS_MODULE, 
	.open = pt100_open,
	.release = pt100_release,
	.read = pt100_read,
	.poll = pt100_poll,
};

struct pt100_data *init_data(dev_t devno)
{
	int err;
	struct pt100_data *data = kzalloc(sizeof(struct pt100_data), GFP_KERNEL);
	if(!data)
	{
		return NULL;
	}
	data->devno = devno;
	data->debugfs = debugfs_create_file(MODULE_NAME, S_IRUGO, NULL, NULL, &pt100_fops);
	if(IS_ERR(data->debugfs))
	{
		err = PTR_ERR(data->debugfs);
		PDEBUG("debugfs_create_file failed\n");
		goto out;
	}
	err = kfifo_alloc(&data->fifo, FIFO_SIZE, GFP_KERNEL);
	if(err)
	{
		PDEBUG("kfifo alloc failed\n");
		goto err_fifo;
	}
	data->myfifo = &data->fifo;
	data->devno = devno;
	cdev_init(&data->chr_cdev, &pt100_fops);
	data->chr_cdev.owner = THIS_MODULE;
	data->chr_cdev.ops = &pt100_fops;
	err = cdev_add(&data->chr_cdev, data->devno, DEIVCE_NUM);
	if(err)
	{
		goto err_cdev;
	}
	
	init_waitqueue_head(&data->wq);
	mutex_init(&data->mutex_lock);
	spin_lock_init(&data->spin_lock);
	
	init_timer(&myuart_timer);
	myuart_timer.data = 0;
	myuart_timer.function = myuart_timer_func;
	myuart_timer.expires = jiffies + HZ;
	add_timer(&myuart_timer);
	tasklet_init(&myuart_tasklet, myuart_tasklet_func, (unsigned long)data);
	PDEBUG("%s success\n", __FUNCTION__);
	return data;

err_cdev:
	kfifo_free(data->myfifo);
err_fifo:
	debugfs_remove(data->debugfs);
out:
	kfree(data);
	return NULL;
}

static int pt100_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	dev_t devno;
	struct pt100_data *pt100_data;
	

	PDEBUG("%s INVOKED\n", __FUNCTION__);
 	
 	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
 	{
		PDEBUG("i2c check functionality failed\n");
		return -ENODEV;
	}

	if(pt100_major)
	{
		devno = MKDEV(pt100_major, pt100_minor);
		err = register_chrdev_region(devno, DEIVCE_NUM, MODULE_NAME);
		if(err)
			return -1;
	}	
	else
	{
		err = alloc_chrdev_region(&devno, 0, DEIVCE_NUM, MODULE_NAME);
		if(err)
			return -1;
		pt100_major = MAJOR(devno);
		PDEBUG("pt100 alloc major %d\n", pt100_major);
	}
	if(err)
	{
		PDEBUG("register chrdev failed\n");
		return -1;
	}
 
	pt100_data = init_data(devno);
	if(NULL == pt100_data)
	{
		unregister_chrdev_region(devno, DEIVCE_NUM);
		return -1;
	}
	pt100_data->client = client;

	i2c_set_clientdata(client, pt100_data);

	

	PDEBUG("%s success\n", __FUNCTION__);
	return 0;
}

static int pt100_remove(struct i2c_client *client)
{
	struct pt100_data *data = i2c_get_clientdata(client);
	del_timer_sync(&myuart_timer);
	cdev_del(&data->chr_cdev);
	unregister_chrdev_region(data->devno, DEIVCE_NUM);
	debugfs_remove(data->debugfs);
	kfifo_free(data->myfifo);
	kfree(data);
	PDEBUG("pt100 remove\n");
	return 0;
}
static struct i2c_driver pt100_driver = {
	.driver = {
		.name	= "PT100",
		.owner = THIS_MODULE,
	},
	.probe		= pt100_probe,
	.remove		= pt100_remove,
	.id_table	= pt100_id,
};


static int __init pt100_init(void)
{	
	PDEBUG("module init\n");
	return i2c_add_driver(&pt100_driver);
}

module_init(pt100_init);

static void __exit pt100_exit(void)
{
	PDEBUG("module exit\n");
	i2c_del_driver(&pt100_driver);
}
module_exit(pt100_exit);

MODULE_AUTHOR("cfn");
MODULE_DESCRIPTION("sensor driver for pt100");
MODULE_LICENSE("GPL");
