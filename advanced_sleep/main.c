#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <asm/hardirq.h>
#include "main.h"

static struct sleep_dev *my_dev;
int sleep_major = 0;
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
struct timer_list timer;
int timer_data = 0;
void timer_handler(unsigned long data)
{
    PDEBUG(": %s()\n",__FUNCTION__);
    PDEBUG(": data %d\n",*((int *)data));
}

int sleep_open(struct inode *inode, struct file *filp)
{
    struct sleep_dev *dev;
    //struct timeval tv;
    //struct timespec ts;
    unsigned long j;
    #if 1
    
    init_timer(&timer);
    timer.expires = jiffies + 2*HZ;
    timer.function = timer_handler;
    timer.data = (unsigned long)&timer_data;
    add_timer(&timer);
    #endif
    timer_data = 0x55;

    j = jiffies;
    dev = container_of(inode->i_cdev,struct sleep_dev,cdev);
    filp->private_data = dev;
    PDEBUG(": sleep_open invoked, jiffies %ld\n",j);

    PDEBUG(": in interrupt %s", in_interrupt() ? "TRUE" : "FALSE");

    #if 0
    /*First method*/
    PDEBUG(": Use schedule() to delay\n");
    PDEBUG(": jiffies %ld\n",jiffies);
    while(time_before(jiffies, j+HZ))
        schedule();
    PDEBUG(": jiffies %ld\n",jiffies);
    /*Second method*/
    PDEBUG(": Use schedule_timeout() to delay\n");
    PDEBUG(": jiffies %ld\n",jiffies);
    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(HZ);
    PDEBUG(": jiffies %ld\n",jiffies);
    /*short delay, non-busy wait*/
    PDEBUG(": NO busy wait");
    PDEBUG(": jiffies %ld\n",jiffies);
    msleep(10); // no interruptible
    PDEBUG(": jiffies %ld\n",jiffies);
    msleep_interruptible(20);
    PDEBUG(": jiffies %ld\n",jiffies);
    ssleep(1);
    PDEBUG(": jiffies %ld\n",jiffies);
    PDEBUG(": sleep_open dealy done\n");
    #endif
    #if 0
    ts.tv_sec = 10;
    ts.tv_nsec = 0;

    tv.tv_sec = 10;
    tv.tv_usec = 0;

    j = timespec_to_jiffies(&ts);
    PDEBUG("timespec.tv_sec = %ld tv_nsec = %ld, jiffies = %ld\n", ts.tv_sec, ts.tv_nsec, j);
    j = timeval_to_jiffies(&tv);
    PDEBUG("timespec.tv_sec = %ld tv_nsec = %ld, jiffies = %ld\n", tv.tv_sec, tv.tv_usec, j);

    j = jiffies;
    memset(&ts, 0, sizeof(struct timespec));
    memset(&tv, 0, sizeof(struct timeval));
    jiffies_to_timespec(j, &ts);
    PDEBUG("timespec.tv_sec = %ld tv_nsec = %ld, jiffies = %ld\n", ts.tv_sec, ts.tv_nsec, j);
    jiffies_to_timeval(j, &tv);
    PDEBUG("timeval.tv_sec = %ld tv_usec = %ld, jiffies = %ld\n", tv.tv_sec, tv.tv_usec, j);

    do_gettimeofday(&tv);
    PDEBUG("do_gettimeofday timeval.tv_sec = %ld tv_usec = %ld\n", tv.tv_sec, tv.tv_usec);
    ts = current_kernel_time();
    PDEBUG("current_kernel_time timespec.tv_sec = %ld tv_nsec = %ld\n", ts.tv_sec, ts.tv_nsec);
    #endif
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
    .fasync = sleep_fasync,
};

static int __init m_init(void)
{
	dev_t dev_id;
	int retval,devno,err;

    retval = alloc_chrdev_region(&dev_id,0,DEVICE_COUNT,MODULE_NAME);
    sleep_major = MAJOR(dev_id);

    if(retval)
    {
    	PDEBUG(": register sleep dev failed\n");
    	return -1;
    }

	my_dev = kmalloc(sizeof(struct sleep_dev), GFP_KERNEL);
	if(!my_dev)
	{
		PDEBUG(":kmalloc failed on sleep_struct\n");
	    return -1;
    }
    memset(my_dev, 0, sizeof(struct sleep_dev));
    init_waitqueue_head(&my_dev->w_queue);
    init_waitqueue_head(&my_dev->r_queue);
    sema_init(&my_dev->sem, 1);
    my_dev->w_pos = 0;
    my_dev->r_pos = 0;
    atomic_set(&my_dev->wait_flag, 0);
    cdev_init(&my_dev->cdev, &sleep_fops);
	
    devno = MKDEV(sleep_major, 0);
    err = cdev_add(&my_dev->cdev, devno, 1);
    if (err)
    {
        kfree(my_dev);
        my_dev = NULL;
        PDEBUG(": Error %d adding sleep", err);
    }
    PDEBUG("m_init done\n");
	return 0;
}

static void __exit m_exit(void)
{
	dev_t dev_id = MKDEV(sleep_major,0);
	cdev_del(&my_dev->cdev);
    kfree(my_dev);
	unregister_chrdev_region(dev_id,1);
	PDEBUG(": Bye, sleep!\n");
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cfn");
MODULE_DESCRIPTION("learning program");
