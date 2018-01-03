#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#ifdef S3C2440
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#endif
#include "fops.h"
#include "main.h"

int ioctl_open(struct inode *inode, struct file *filp)
{
	static unsigned char only_once = 1;
	if(only_once)
	{
		#ifdef S3C2440
		s3c2410_gpio_cfgpin(S3C2410_GPF4, S3C2410_GPIO_OUTPUT);
		s3c2410_gpio_cfgpin(S3C2410_GPF5, S3C2410_GPIO_OUTPUT);
		s3c2410_gpio_cfgpin(S3C2410_GPF6, S3C2410_GPIO_OUTPUT);
		#endif
	}
	filp->private_data  = container_of(inode->i_cdev,struct ioctl_dev,cdev);
	PDEBUG(": %s() is invoked\n",__FUNCTION__);
	return 0;
}

int ioctl_release(struct inode *inode, struct file *filp)
{
	PDEBUG(": %s() is invoked\n",__FUNCTION__);
	return 0;
}

ssize_t ioctl_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{	

	struct ioctl_dev *ioctl_dev;
	unsigned char ret = 0;
	ioctl_dev = filp->private_data;

	PDEBUG(": %s() is invoked\n",__FUNCTION__);

	if(mutex_lock_interruptible(&ioctl_dev->ioctl_mutex))
        return -ERESTARTSYS;
	
    while(!ioctl_dev->current_len)
	{
		mutex_unlock(&ioctl_dev->ioctl_mutex);
		if(filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		PDEBUG("ioctl read current len is zero, gooing to sleep\n");
		wait_event_interruptible(ioctl_dev->r_wait, ioctl_dev->current_len);
		
		if(mutex_lock_interruptible(&ioctl_dev->ioctl_mutex))
            return -ERESTARTSYS;
	}
	
	if(count > ioctl_dev->current_len - *f_pos)
		count = ioctl_dev->current_len - *f_pos;
	if(copy_to_user(buf, ioctl_dev->mem + *f_pos, count))
	{
		mutex_unlock(&ioctl_dev->ioctl_mutex);
		PDEBUG("copy to user failed\n");
		return -1;
	}
	
	*f_pos += count;
	if(*f_pos >= ioctl_dev->current_len)
	{
		*f_pos = 0;
		ioctl_dev->current_len = 0;
		wake_up_interruptible(&ioctl_dev->w_wait);
		PDEBUG("buf read all done,can write agagin now\n");
	}
	
	#ifdef S3C2440

	ret = s3c2410_gpio_getpin(S3C2410_GPF4);
    PDEBUG("ret %#x\n", ret);
	ret |= (s3c2410_gpio_getpin(S3C2410_GPF5) << 1);
    PDEBUG("ret %#x\n", ret);
	ret |= (s3c2410_gpio_getpin(S3C2410_GPF6) << 2);
    PDEBUG("ret %#x\n", ret);
	#else
	ret = 0x0;	
	#endif 
	
	if(copy_to_user(buf, &ret, 1))
	{
		PDEBUG("copy user failed\n");
		return -1;
	}
	PDEBUG("read ok\n");
	mutex_unlock(&ioctl_dev->ioctl_mutex);
	return count;	
}

ssize_t ioctl_write(struct file *filp, const char  __user *buf, size_t count,loff_t *f_pos)
{
	unsigned char data = 0;
	unsigned int i = 0;
	struct ioctl_dev *ioctl_dev;
	ioctl_dev = filp->private_data;
	
	PDEBUG(": %s() is invoked\n",__FUNCTION__);
	if(mutex_lock_interruptible(&ioctl_dev->ioctl_mutex))
        return -ERESTARTSYS;
	while(ioctl_dev->current_len)
	{
		mutex_unlock(&ioctl_dev->ioctl_mutex);
		if(filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		PDEBUG("ioctl read current len is %d, gooing to sleep\n",ioctl_dev->current_len);
		wait_event_interruptible(ioctl_dev->w_wait, !ioctl_dev->current_len);	
		if(mutex_lock_interruptible(&ioctl_dev->ioctl_mutex))
            return -ERESTARTSYS;
	}
	if(count > IOCTL_BUF_LEN - *f_pos)
		count = IOCTL_BUF_LEN - *f_pos;
	if(copy_from_user(ioctl_dev->mem + *f_pos, buf, count))
	{
		PDEBUG("copy from user first failed\n");
		mutex_unlock(&ioctl_dev->ioctl_mutex);
		return -1;
	}
	if(count > 0)
	{
		ioctl_dev->current_len += count;
		wake_up_interruptible(&ioctl_dev->r_wait);
		*f_pos = 0;
	}
	
	if(copy_from_user(&data, buf, 1))
	{
		PDEBUG("copy from user failed\n");
		mutex_unlock(&ioctl_dev->ioctl_mutex);
		return -1;
	}
	for(i = 0; i < ioctl_dev->current_len; i++)
	{
		PDEBUG("mem %#x\n", *(ioctl_dev->mem + i));
	}
	
	PDEBUG("data %#x\n",data);
	PDEBUG("ioctl current_len = %d, count = %zd\n",ioctl_dev->current_len, count);

	#ifdef S3C2440 
	s3c2410_gpio_setpin(S3C2410_GPF4, data&(1 << 0) ? 1 : 0);
	s3c2410_gpio_setpin(S3C2410_GPF5, data&(1 << 1) ? 1 : 0);
	s3c2410_gpio_setpin(S3C2410_GPF6, data&(1 << 2) ? 1 : 0);
	#endif
	mutex_unlock(&ioctl_dev->ioctl_mutex);
	PDEBUG("write ok\n");
	return count;
}
long ioctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0,tmp = 0;
	int retval = 0;
	unsigned int led = 0;
	struct ioctl_dev *ioctl_dev;
	ioctl_dev = file->private_data;
	PDEBUG(": %s invokedn",__FUNCTION__);
	if(_IOC_TYPE(cmd) != IOCTL_IOC_MAGIC)
	{
		PDEBUG("TYPE not right\n");
		return -EFAULT;
	}

	if(_IOC_NR(cmd) > IOCTL_IOC_MAXNR)
	{
		PDEBUG("NR not right\n");
		return -EFAULT;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
    	err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
    	err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    if (err) return -EFAULT;

    switch(cmd)
   	{
   		case IOCTL_IOCRESET:
   			ioctl_dev->quantum = IOCTL_QUANTUM;
   			ioctl_dev->qset = IOCTL_QSET;
   			break;

   		case IOCTL_IOCSQUANTUM:/* Set: arg points to the value */
   			if(!capable(CAP_SYS_ADMIN))
   				return -EPERM;
   			retval = __get_user(ioctl_dev->quantum,(int __user*)arg);
   			break;

		case IOCTL_IOCSQSET: /* Set: arg points to the value */
   			if(!capable(CAP_SYS_ADMIN))
   				return -EPERM;
			retval = __get_user(ioctl_dev->qset,(int __user*)arg);
			break;

		case IOCTL_IOCTQUANTUM:
			if(!capable(CAP_SYS_ADMIN))
   				return -EPERM;
   			ioctl_dev->quantum = arg;
			break;

		case IOCTL_IOCTQSET:
			if(!capable(CAP_SYS_ADMIN))
	   			return -EPERM;
   			ioctl_dev->qset = arg;
			break;

		case IOCTL_IOCGQUANTUM:
			retval = __put_user(ioctl_dev->quantum, (int __user *)arg);
			break;

		case IOCTL_IOCGQSET:
			retval = __put_user(ioctl_dev->qset, (int __user *)arg);
			break;

		case IOCTL_IOCQQUANTUM:
			retval = ioctl_dev->quantum;
			break;

		case IOCTL_IOCQQSET:
			retval = ioctl_dev->qset;
			break;

		case IOCTL_IOCXQUANTUM:
			if(!capable(CAP_SYS_ADMIN))
	   			return -EPERM;
	   		tmp = ioctl_dev->quantum;
	   		retval = __get_user(ioctl_dev->quantum,(int __user*)arg);
	   		if(retval == 0)
	   			retval = __put_user(tmp,(int __user*)arg);
			break;

		case IOCTL_IOCXQSET:
			if(!capable(CAP_SYS_ADMIN))
	   			return -EPERM;
	   		tmp = ioctl_dev->qset;
	   		retval = __get_user(ioctl_dev->qset,(int __user*)arg);
	   		if(retval == 0)
	   			retval = __put_user(tmp,(int __user*)arg);
			break;

		case IOCTL_IOCHQUANTUM:
			if(!capable(CAP_SYS_ADMIN))
	   			return -EPERM;
	   		tmp = ioctl_dev->quantum;
	   		ioctl_dev->quantum = arg;
	   		retval = tmp;
			break;
		#ifdef S3C2440 	
		case LEDCTL_ON:
			s3c2410_gpio_setpin(S3C2410_GPF4, 0);
			s3c2410_gpio_setpin(S3C2410_GPF5, 0);
			s3c2410_gpio_setpin(S3C2410_GPF6, 0);
			break;
			
		case LEDCTL_OFF:
			s3c2410_gpio_setpin(S3C2410_GPF4, 1);
			s3c2410_gpio_setpin(S3C2410_GPF5, 1);
			s3c2410_gpio_setpin(S3C2410_GPF6, 1);
			break;

		case LEDCTL_GET:
			led = s3c2410_gpio_getpin(S3C2410_GPF4);
			led |= (s3c2410_gpio_getpin(S3C2410_GPF5) << 1);
			led |= (s3c2410_gpio_getpin(S3C2410_GPF6) << 2);
			retval = __put_user(led,(unsigned int __user*)arg);
			PDEBUG("get led %#x\n",led);
			break;
			
		case LEDCTL_SET:
			retval = __get_user(led,(unsigned int __user*)arg);
			PDEBUG("set led %#x\n",led);
			if(0 == retval)
			{
				s3c2410_gpio_setpin(S3C2410_GPF4, led & 0x01);
				s3c2410_gpio_setpin(S3C2410_GPF5, led & 0x02);
				s3c2410_gpio_setpin(S3C2410_GPF6, led & 0x04);
			}
			break;
		#else 
		case LEDCTL_ON:
			PDEBUG("LED ON\n");
			break;
		
		case LEDCTL_OFF:
			PDEBUG("LED OFF\n");
			break;
		
		case LEDCTL_GET:
			retval = __put_user(led,(unsigned int __user*)arg);
			PDEBUG("get led %#x\n",led);
			break;
			
		case LEDCTL_SET:
			retval = __get_user(led,(unsigned int __user*)arg);
			PDEBUG("set led %#x\n",led);
			if(0 != retval)
				PDEBUG("get user failed %#x\n",retval);
			break;
		#endif
		default:
			break;
   	}
   	return retval;
}
