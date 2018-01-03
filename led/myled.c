#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include "myled.h"

#define MODULE_NAME 	"myled"
#define PDEBUG(format, args...) printk(KERN_WARNING MODULE_NAME ": " format, ##args)

#if 0
static int led_major = 0, led_minor = 0;
module_param(led_major, int, 0);
module_param(led_minor, int, 0);
#endif

static struct mutex led_mutex;  

static unsigned long GPIO_NR_PORTS = 0xA0;

static void __iomem *map;
volatile unsigned int *led_mem;

int set_gpio_level (unsigned int gpio_n, unsigned char lev,  unsigned int *out) 
{
 
    *out = 0;
    /* GPIO pin n is HIGH */              
    if (lev == 1) 
    {               
        if (gpio_n < 32) 
        {
            /* sets the respective bit */                 
            *out |= (1 << gpio_n);
            iowrite32(*out, led_mem + BCM2837_GPLEV0);

        } 
        else 
        {
            /*set the bits 0 - 21 for GPIO greater than 31 */                 
            *out |= (1 << (gpio_n - 32));
            iowrite32(*out, led_mem + BCM2837_GPLEV1);
        }

    } 
    else 
    {
        if (gpio_n < 32) 
        {
            /* sets the respective bit */                 
            *out &= ~(1 << gpio_n);
            iowrite32(*out, led_mem + BCM2837_GPLEV0);
        } 
        else 
        {
            /*set the bits 0 - 21 for GPIO greater than 31 */ 
            *out &= ~(1 << (gpio_n - 32));
            iowrite32(*out, led_mem + BCM2837_GPLEV1);
        }
    }
    return 0;
  
}
int set_gpio(unsigned int gpio_n, unsigned int *out) 
{
  *out = 0;
    PDEBUG("map %#x res  %#x\n", map, map + BCM2837_GPSET0*4);
    PDEBUG("led_mem %#x res %#x\n", led_mem, led_mem + BCM2837_GPSET0);

  if (gpio_n < 32) {
      *out |= 1 << gpio_n;
    //*(led_mem + BCM2837_GPSET0) = *out;
    writel(*out, (map+BCM2837_GPSET0*4));

    } else {
      *out |= 1 << (gpio_n - 32); 
    //*(led_mem + BCM2837_GPSET1) = *out;
    writel(*out, (map+BCM2837_GPSET1));
    }


  return 0;
}/* set_gpio */
/**
 * @Description: 
 * 000 = GPIO Pin n is an input
 * 001 = GPIO Pin n is an output
 * 100 = GPIO Pin n takes alternate function 0 
 * 101 = GPIO Pin n takes alternate function 1 
 * 110 = GPIO Pin n takes alternate function 2 
 * 111 = GPIO Pin n takes alternate function 3 
 * 011 = GPIO Pin n takes alternate function 4 
 * 010 = GPIO Pin n takes alternate function 5
 * @param          : gpio number
 * @param          : respective GPIO is set OUTPUT
 * @return         : Always return 0
 **/
int set_gpio_out (unsigned int gpio_n, unsigned int *out) 
{

    *out = 0;
    /* sets the respective bit */                 
    *out |= (1 << (gpio_n % 10) * 3);
    PDEBUG("map %#x res  %#x\n", map, map + (gpio_n / 10)*4);
    PDEBUG("led_mem %#x res %#x\n", led_mem, led_mem + (gpio_n / 10));
    writel(*out, (map + (gpio_n / 10)*4));
    //*(led_mem + (gpio_n / 10)) = *out;
    return 0;
}/* set_gpio_out */
unsigned int gpio_read (unsigned int gpio_n) 
{
                
    if (gpio_n < 32)
    {   
        return (ioread32(led_mem + BCM2837_GPLEV0)&(1 << gpio_n));
    } 
    else 
    {
        return (ioread32(led_mem + BCM2837_GPLEV1)&(1 << (gpio_n - 32)));
    }
    return 0;

}/* gpio_read*/
int led_open (struct inode *i_node, struct file *fp)
{
	mutex_lock_interruptible(&led_mutex);
	return 0;
}
int led_release (struct inode *i_node, struct file *fp)
{
	mutex_unlock(&led_mutex);
	return 0;
}
ssize_t led_read (struct file *fp, char __user *buf, size_t len, loff_t *offset)
{
    return len;
}
ssize_t led_write (struct file *fp, const char __user *buf, size_t len, loff_t *offset)
{
    return len;
}

static struct file_operations led_fops = {
	.owner   = THIS_MODULE,
	.open    = led_open,
	.read    = led_read,
	.write   = led_write,
	.release = led_release,
};

static struct miscdevice led_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MODULE_NAME,
	.fops = &led_fops,
};


static int __init led_init(void)
{
	int err = 0;
    unsigned int out;
	PDEBUG(" is loaded\n");
	err = misc_register(&led_miscdev);
	if(err)
	{
		PDEBUG("misc register failed\n");
        return err;
	}
	mutex_init(&led_mutex);
    #if 0
	if (!request_mem_region(BCM2837_GPIO_BASE, GPIO_NR_PORTS, "myled")) 
	{
		PDEBUG("can't get I/O mem address %#x\n",BCM2837_GPIO_BASE);
		return -ENODEV;
	}
    #endif
	map = ioremap(BCM2837_GPIO_BASE, GPIO_NR_PORTS);
	if(!map)
	{
        misc_deregister(&led_miscdev);
		PDEBUG("ioremap failed\n");
        return -ENODEV;
	}
    led_mem    = (volatile unsigned int *)map;
    set_gpio_out(26, &out);
    PDEBUG("set sgpio out\n");
    out = 1;
    set_gpio(26, &out);
    
	return err;
}

static void __exit led_exit(void)
{
	misc_deregister(&led_miscdev);
	//release_mem_region(BCM2837_GPIO_BASE, GPIO_NR_PORTS);
	iounmap(led_mem);
}
module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cfn");
MODULE_DESCRIPTION("A simple irq module");
