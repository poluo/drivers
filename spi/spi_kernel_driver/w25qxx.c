/*
* @Author: poluo
* @Date:   2018-01-04 14:39:38
* @Last Modified by:   changfengnan
* @Last Modified time: 2018-01-04 16:54:49
*/

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/debugfs.h>

#define PDEBUG(format, args...) printk(KERN_WARNING MODULE_NAME "dev: " format, ##args)
#define MODULE_NAME "w25qxx"
#define DEVICE_NAME		1
#define USE_MODE	1 // 1 chrdev, 0 auto test
#define W25Q64_CHIP_ID	0x55
#define W25Q128_CHIP_ID	0x56
#define W25Q256_CHIP_ID	0x57

#define RX_BUF_SIZE 	256
#define TX_BUF_SIZE		256


#define SR1_BUSY_MASK   0x01
#define SR1_WEN_MASK    0x02
#define CMD_CHIP_ERASE        0xC7
#define CMD_SECTOR_ERASE      0x20
#define CMD_WRIRE_ENABLE      0x06
#define CMD_READ_DATA         0x03
#define CMD_FAST_READ         0x0B
#define CMD_READ_STATUS_R1    0x05
#define CMD_PAGE_PROGRAM      0x02
#define CMD_JEDEC_ID          0x9f
struct w25qxx_info{
	struct spi_device *device;
	struct mutex  lock;
	dev_t devno;
	struct cdev chr_dev;
	struct dentry *debugfs;
};
static int w25qxx_open(struct inode *inode, struct file *file)
{
	struct w25qxx_info *dev_info = container_of(inode->i_cdev, struct w25qxx_info, chr_dev);

	if(dev_info == NULL)
		return -ENODEV;
	file->private_data = dev_info;
	return 0;
}
bool W25Q64_IsBusy(struct spi_device *spi_dev) 
{

	uint8_t tx[2], rx[2];
  	int err;
  	uint8_t r1;
  	tx[0] = CMD_READ_STATUS_R1;
  	tx[1] = 0xff;
 	err = spi_write_then_read(spi_dev, tx, 2, rx, 1);
    r1 = rx[0];

    if(r1 & SR1_BUSY_MASK)
    {
    	return true;
    }
  	return false;
}
static ssize_t w25qxx_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct w25qxx_info *dev_info = file->private_data;
	struct spi_device *spi_dev = dev_info->device;
	struct spi_transfer xfer[2];
	struct spi_message message;
	int err;
	unsigned int addr = (unsigned int)*ppos;
	unsigned char tx_buf[4];
	unsigned char *rx_buf;

	
	PDEBUG("%s invoked,addr = %#x\n", __FUNCTION__,addr);

	spi_message_init(&message);
	memset(xfer, 0, sizeof(xfer));

	rx_buf = kzalloc(count, GFP_KERNEL);
	if(rx_buf == NULL)
	{
		PDEBUG("kzalloc for rx buf failed\n");
		return -ENOMEM;
	}	

	tx_buf[0] = CMD_READ_DATA;
	tx_buf[1] = (addr>>16) & 0xFF;     // A23-A16
  	tx_buf[2] = (addr>>8) & 0xFF;      // A15-A08
  	tx_buf[3] = addr & 0xFF;           // A07-A00

	
	xfer[0].tx_buf = tx_buf;
	xfer[0].len = 4;
	spi_message_add_tail(&xfer[0], &message);

	xfer[1].rx_buf = rx_buf;
	xfer[1].len = count;
	spi_message_add_tail(&xfer[1], &message);

	err = spi_sync(spi_dev, &message);
	if(unlikely(err))
	{
		PDEBUG("spi_sync failed\n");
		return err;
	}

	err = copy_to_user(buf, rx_buf, count);
	if (err != 0)
	{
		PDEBUG("copy_to_user failed\n");
		return err;
	}	

	return count;
}
static ssize_t w25qxx_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct w25qxx_info *dev_info = file->private_data;
	struct spi_device *spi_dev = dev_info->device;
	struct spi_transfer xfer[2];
	struct spi_message message, message2;
	unsigned int command = CMD_WRIRE_ENABLE;
    unsigned char command2[4];
	int err,retry = 0;
	unsigned int inaddr = (unsigned int)*ppos, addr = 0;
	unsigned char *tx_buf;

	PDEBUG("%s invoked,addr = %#x\n", __FUNCTION__,addr);

	spi_message_init(&message);
	spi_message_init(&message2);
	memset(xfer, 0, sizeof(xfer));

	tx_buf = kzalloc(count + 4, GFP_KERNEL);
	if(tx_buf == NULL)
	{
		PDEBUG("kzalloc for rx buf failed\n");
		return -ENOMEM;
	}
	if(W25Q64_IsBusy(spi_dev))
	{
		msleep(20);
		kfree(tx_buf);
		return 0;
	}

	xfer[0].tx_buf = &command;
	xfer[0].len = 1;
	spi_message_add_tail(&xfer[0], &message);
	command2[0] = CMD_SECTOR_ERASE;
    command2[1] = (addr >> 16);
    command2[2] = (addr >> 8);
    command2[3] = (addr);
    xfer[1].tx_buf = command2;
    xfer[1].len = 1;
    spi_message_add_tail(&xfer[1], &message);

    err = spi_sync(spi_dev, &message);
    if(unlikely(err))
    {
        PDEBUG("spi_sync failed\n");
        return err;
    }

    memset(xfer, 0, sizeof(xfer));
    while(W25Q64_IsBusy(spi_dev))
    {
        if(retry++ > 10)
        {
            PDEBUG("w25q is busy,count more than 10\n");
            return 0;
        }
        msleep(20);
        return 0;
    }

    xfer[0].tx_buf = &command;
	xfer[0].len = 1;
	spi_message_add_tail(&xfer[0], &message2);
	tx_buf[0] = CMD_PAGE_PROGRAM;
	tx_buf[1] = (addr>>16) & 0xFF;     // A23-A16
  	tx_buf[2] = (addr>>8) & 0xFF;      // A15-A08
  	tx_buf[3] = addr & 0xFF;           // A07-A00
  	addr <<= 12;
	addr += inaddr;
	err = copy_from_user(&tx_buf[4], buf, count);
	if(err)
	{
		kfree(tx_buf);
		PDEBUG("copy_from_user failed\n");
		return -EFAULT;
	}

	xfer[1].tx_buf = tx_buf;
	xfer[1].len = count + 4;
	spi_message_add_tail(&xfer[1], &message2);

	err = spi_sync(spi_dev, &message2);
	if(unlikely(err))
	{
		kfree(tx_buf);
		PDEBUG("spi_sync 2 failed\n");
		return err;
	}
	kfree(tx_buf);
	return count;
}
static int w25qxx_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static const struct file_operations w25qxx_fops =
{
	.owner = THIS_MODULE, 
	.open = w25qxx_open,
	.release = w25qxx_release,
	.read = w25qxx_read,
	.write = w25qxx_write,
};
void w25qxx_read_id(struct spi_device *spi_dev)
{
	struct spi_transfer xfer[2];
	struct spi_message message;
	int err;
	unsigned char command = CMD_JEDEC_ID;
	unsigned char *rx_buf;

	
	PDEBUG("%s invoked", __FUNCTION__);

	spi_message_init(&message);
	memset(xfer, 0, sizeof(xfer));

	rx_buf = kzalloc(4, GFP_KERNEL);
	if(rx_buf == NULL)
	{
		PDEBUG("kzalloc for rx buf failed\n");
		return;
	}	
	
	xfer[0].tx_buf = &command;
	xfer[0].len = 1;
	spi_message_add_tail(&xfer[0], &message);

	xfer[1].rx_buf = rx_buf;
	xfer[1].len = 4;
	spi_message_add_tail(&xfer[1], &message);

	err = spi_sync(spi_dev, &message);
	if(unlikely(err))
	{
		PDEBUG("spi_sync failed\n");
		return;
	}

	PDEBUG("CMD_JEDEC_ID %#x %#x %#x %#x\n", rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);
}
static int w25qxx_spi_probe(struct spi_device *spi)
{
	int err = 0;
	struct w25qxx_info  *dev_info;
	const struct spi_device_id *id = spi_get_device_id(spi);
	PDEBUG("spi probe\n");
	PDEBUG("spi device name %s, driver data %#x\n", id->name, (unsigned int)id->driver_data);
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	err = spi_setup(spi);

	if(err < 0)
	{
		PDEBUG("spi setup failed\n");
		return err;
	}
	PDEBUG("spi setup ok\n");
	dev_info = kzalloc(sizeof(struct w25qxx_info), GFP_KERNEL);
	if(dev_info == NULL)
	{
		PDEBUG("out of mem\n");
		return -ENOMEM;
	}	
	dev_info->device = spi;
	dev_info->debugfs = debugfs_create_file(MODULE_NAME, S_IRUGO, NULL, NULL, &w25qxx_fops);
	if(IS_ERR(dev_info->debugfs))
	{
		err = PTR_ERR(dev_info->debugfs);
		PDEBUG("debugfs alloc failed");
		return err;
	}
	err = alloc_chrdev_region(&dev_info->devno, 0, DEVICE_NAME, MODULE_NAME);
	if(err)
	{
		PDEBUG("alloc chrdev failed\n");
	}
	cdev_init(&dev_info->chr_dev, &w25qxx_fops);
	err = cdev_add(&dev_info->chr_dev, dev_info->devno, DEVICE_NAME);
	if(err)
	{
		PDEBUG("cdev_add failed\n");
	}
	mutex_init(&dev_info->lock);
	spi_set_drvdata(spi, dev_info);
	w25qxx_read_id(spi);

	return 0;
}
static int w25qxx_spi_remove(struct spi_device *spi)
{
	PDEBUG("spi remove\n");
	return 0;
}
static const struct of_device_id w25qxx_of_spi_match[] = 
{
	{ .compatible = "m25p80", },
	{ .compatible = "w25q128", },
	{ .compatible = "w25q256", },
	{ }
};

static const struct spi_device_id w25qxx_spi_id[] = 
{
	{"w25q64", W25Q64_CHIP_ID},
	{"w25q128", W25Q128_CHIP_ID},
	{"w25q256", W25Q256_CHIP_ID},
	{ }
};

static struct spi_driver w25qxx_spi_driver = 
{
	.driver = {
		.owner = THIS_MODULE,
		.name = "w25qxx",
		.of_match_table = w25qxx_of_spi_match,
	},
	.id_table = w25qxx_spi_id,
	.probe = w25qxx_spi_probe,
	.remove = w25qxx_spi_remove,
};


module_spi_driver(w25qxx_spi_driver);

MODULE_DESCRIPTION("W25Qxx SPI bus driver");
MODULE_LICENSE("GPL");
