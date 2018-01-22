#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/platform_device.h>


#define MODULE_NAME	"all"

#define PDEBUG(format, args...) printk(KERN_WARNING MODULE_NAME ": " format, ##args)
#define SOURCE_SIZE		256

static struct platform_device *sleep0;
int init_uarts(void)
{
	sleep0 = platform_device_alloc(MODULE_NAME, 0);
	if(!sleep0)
	{
		PDEBUG("platform_device_alloc failed\n");
		return -ENOMEM;
	}
	PDEBUG("init_uarts success\n");
	return 0;
}

static int __init m_init(void)
{
	int err = 0;
	PDEBUG(" is loaded\n");

	init_uarts();

	err = platform_device_add(sleep0);
	if(err)
	{
		PDEBUG("add device failed\n");
		return err;
	}

	return err;
}

static void __exit m_exit(void)
{
	PDEBUG(" unloaded\n");

	platform_device_unregister(sleep0);

}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cfn");
MODULE_DESCRIPTION("A simple platform module");
