#include <linux/module.h>
#include <linux/spi/spi.h>

#define PDEBUG(format, args...) printk(KERN_WARNING MODULE_NAME ": " format, ##args)
#define DEV_NAME "w25qxx"
#define MODULE_NAME "spidev"
#define BUS_NUM 	0

static struct spi_device *spi_device;
struct spi_master *master;

static int __init spidev_module_init(void)
{
	
	struct spi_board_info spi;

	strcpy(spi.modalias, DEV_NAME);
	spi.mode = SPI_MODE_0;
	spi.chip_select = 0;
	spi.bus_num = BUS_NUM;
	spi.max_speed_hz = 2 * 1000 * 1000;;

	master = spi_busnum_to_master(BUS_NUM);
	if (!master) 
	{
		PDEBUG("spi_busnum_to_master(%d) returned NULL\n", BUS_NUM);
		return -EINVAL;
	}
	/* make sure it's available */
	spi_device = spi_new_device(master, &spi);
	
	if (!spi_device) 
	{
		spi_master_put(master);
		PDEBUG("spi_new_device() returned NULL\n");
		return -EPERM;
	}

	PDEBUG("spidev init\n");
	return 0;
}

static void __exit spidev_module_exit(void)
{
	spi_unregister_device(spi_device);
	PDEBUG("exited\n");
}

module_init(spidev_module_init);
module_exit(spidev_module_exit);

MODULE_AUTHOR("cfn");
MODULE_DESCRIPTION("PT100 I2C device initializer");
MODULE_LICENSE("GPL");
