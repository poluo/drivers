#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/platform_device.h>


#define MODULE_NAME 	"myuart"
#define PDEBUG(format, args...) printk(KERN_WARNING MODULE_NAME "dev: " format, ##args)
#define SOURCE_SIZE		256

static struct platform_device *myuart0;


int init_uarts(void)
{
	myuart0 = platform_device_alloc(MODULE_NAME, 0);
	if(!myuart0)
	{
		PDEBUG("platform_device_alloc failed\n");
		return -ENOMEM;
	}
	PDEBUG("init_uarts success\n");
	return 0;
}


static int __init myuart_init(void)
{
	int err = 0;
	PDEBUG(" is loaded\n");

	init_uarts();

	err = platform_device_add(myuart0);
	if(err)
	{
		PDEBUG("add device failed\n");
		return err;
	}

	return err;
}

static void __exit myuart_exit(void)
{
	PDEBUG(" unloaded\n");

	platform_device_unregister(myuart0);

}


module_init(myuart_init);
module_exit(myuart_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cfn");
MODULE_DESCRIPTION("A simple platform module");
