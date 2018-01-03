#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/workqueue.h>  

#define MODULE_COUNT	1
#define MODULE_NAME 	"irq"
#define IRQ_NUM 		4
#define PDEBUG(format, args...) printk(KERN_WARNING MODULE_NAME ": " format, ##args)
static int irq_major = 0, irq_minor = 0;

struct irq_dev {
	struct cdev cdev;
	struct tasklet_struct tlet;
    struct work_struct work;
	wait_queue_head_t	read_wait;
	unsigned int data;
	unsigned int key_count[IRQ_NUM];
	unsigned int key_private_data[IRQ_NUM];
};

static struct irq_dev *irq_dev[MODULE_COUNT];

struct irq_table {
	unsigned int irq_num;
	unsigned long flags;
	irq_handler_t handler;
	char *devname;
};

static struct irq_table tab[IRQ_NUM]=
{
	{IRQ_EINT19, IRQF_TRIGGER_FALLING|IRQF_DISABLED, NULL, "KEY1"}, /* K1 */
    {IRQ_EINT11, IRQF_TRIGGER_FALLING|IRQF_DISABLED, NULL, "KEY2"}, /* K2 */
    {IRQ_EINT2,  IRQF_TRIGGER_FALLING|IRQF_DISABLED, NULL, "KEY3"}, /* K3 */
    {IRQ_EINT0,  IRQF_TRIGGER_FALLING|IRQF_DISABLED, NULL, "KEY4"}, /* K4 */
};

static volatile int ev_press = 0;

static void my_tasklet_action(unsigned long data)
{
    struct irq_dev *dev = (struct irq_dev *)data;
    int i = 0;
    for (i = 0; i < IRQ_NUM; ++i)
    {
        PDEBUG("KEY%d count %d\n", i+1,  dev->key_count[i]);
    }
    ev_press = 1;
    wake_up_interruptible(&dev->read_wait);
}

static void work_handler(struct work_struct *arg)
{
    struct irq_dev *dev = container_of(arg, struct irq_dev, work);
    int i;
    for (i = 0; i < IRQ_NUM; ++i)
    {
        PDEBUG("work KEY%d count %d\n", i+1,  dev->key_count[i]);
    }

}
static irqreturn_t my_irq(int irq, void *arg)
{
	struct irq_dev *dev = (struct irq_dev *)arg;
	if(irq == IRQ_EINT19){
		dev->key_count[0]++;
		PDEBUG("%d count %u\n",irq, dev->key_count[0]);
	}
	else if(irq == IRQ_EINT11){
		dev->key_count[1]++;
		PDEBUG("%d count %u\n",irq, dev->key_count[1]);
	}
	else if(irq == IRQ_EINT2){
		dev->key_count[2]++;
		PDEBUG("%d count %u\n",irq, dev->key_count[2]);
	}
	else if(irq == IRQ_EINT0){
		dev->key_count[3]++;
		PDEBUG("%d count %u\n",irq, dev->key_count[3]);
	}
	else{
		PDEBUG("Unknow irq %d\n",irq);
	}
    tasklet_init(&dev->tlet, my_tasklet_action, (unsigned long)dev);
    tasklet_schedule(&dev->tlet);
    schedule_work(&dev->work);
	return IRQ_RETVAL(IRQ_HANDLED);
}


int irq_open(struct inode *inode, struct file *filp)
{
	struct irq_dev *dev;
	dev = container_of(inode->i_cdev, struct irq_dev, cdev);
	filp->private_data = dev;
	PDEBUG("%s invoked\n",__FUNCTION__);
	
	return 0;
}


int irq_release(struct inode *inode, struct file *filp)
{

	PDEBUG("%s invoked\n",__FUNCTION__);
	return 0;
}

ssize_t irq_read(struct file *filp, char __user *buff, size_t count, loff_t *f_ops)
{
	struct irq_dev *dev =filp->private_data;
	int i;
	PDEBUG("%s invoked\n",__FUNCTION__);
	if (count > IRQ_NUM)
	{
		count = IRQ_NUM;
	}
	for (i = 0; i < count; ++i)
	{
		PDEBUG(" %d %d\n", i, dev->key_count[i]);
	}
	#if 0
	while(!ev_press)
	{
		set_current_state(TASK_INTERRUPTIBLE);
    	schedule_timeout(HZ);
	}
	#endif
	wait_event_interruptible(dev->read_wait, ev_press);
	ev_press = 0;
	if(copy_to_user(buff, dev->key_count, count * sizeof(unsigned int)))
	{
		PDEBUG("copy to user failed\n");
	}
	return count;
}


static struct file_operations irq_fops = {
	.owner   = THIS_MODULE,
	.open    = irq_open,
	.read    = irq_read,
	.release = irq_release,
};



static void __init init_irq_dev(struct irq_dev *dev)
{
    int i = 0;
	
    dev->data = 0;

	cdev_init(&dev->cdev, &irq_fops);
	
	dev->cdev.owner = THIS_MODULE;
	init_waitqueue_head(&dev->read_wait);
    for (i = 0; i < IRQ_NUM; ++i)
    {
        dev->key_count[i] = 0;
    }
}

static
int __init m_init(void)
{
	int err = 0, i = 0, j = 0;
	dev_t devno;

	PDEBUG(" is loaded\n");

	//Alloc device number
	err = alloc_chrdev_region(&devno, irq_minor, MODULE_COUNT, MODULE_NAME);
	if (err < 0) {
		PDEBUG("Cant't get major");
		return err;
	}
	irq_major = MAJOR(devno);

	for (i = 0; i < MODULE_COUNT; ++i) 
	{
		irq_dev[i] = kmalloc(sizeof(struct irq_dev), GFP_KERNEL);
		if (!irq_dev[i]) {
			PDEBUG("Error(%d): kmalloc failed on irq%d\n", err, i);
			continue;
		}

		init_irq_dev(irq_dev[i]);

        for (j = 0; j < IRQ_NUM; ++j)
        {
            tab[j].handler = my_irq;
            err = request_irq(tab[j].irq_num, tab[j].handler, tab[j].flags, tab[j].devname, (void *)irq_dev[i]);
            if(err)
            {
                PDEBUG("request_irq for %d failed\n",i);
                break;
            }
        }
        //INIT_WORK(&irq_dev[i]->work, work_handler, (void *)irq_dev[i]);
        INIT_WORK(&irq_dev[i]->work, work_handler);
        if(err)
        {
            for (; j >= 0; j--)
            {
                free_irq(tab[j].irq_num, (void *)irq_dev[i]);
            }
        }
	
		/*
		 * The cdev_add() function will make this char device usable
		 * in userspace. If you havn't ready to populate this device
		 * to its users, DO NOT call cdev_add()
		 */
		devno = MKDEV(irq_major, irq_minor + i);
		err = cdev_add(&irq_dev[i]->cdev, devno, 1);
		if (err) {
			PDEBUG("Error(%d): Adding %s%d error\n", err, MODULE_NAME, i);
			kfree(irq_dev[i]);
			irq_dev[i] = NULL;
		}
	}

	return 0;
}

static
void __exit m_exit(void)
{
	dev_t devno;
	int i, j;
	PDEBUG(" unloaded\n");

    for (i = 0; i < MODULE_COUNT; ++i)
    {
        for (j = 0; j < IRQ_NUM; ++j)
        {
            free_irq(tab[j].irq_num, (void *)irq_dev[i]);
        }
        
    }

	for (i = 0; i < MODULE_COUNT; ++i) 
	{
		cdev_del(&irq_dev[i]->cdev);
		kfree(irq_dev[i]);
	}

	devno = MKDEV(irq_major, irq_minor);
	unregister_chrdev_region(devno, MODULE_COUNT);
}


module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cfn");
MODULE_DESCRIPTION("A simple irq module");
