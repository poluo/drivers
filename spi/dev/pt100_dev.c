#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>

#define PDEBUG(format, args...) printk(KERN_WARNING MODULE_NAME ": " format, ##args)
#define DEV_NAME "PT100"
#define DEV_ADDR 0x5c
#define MODULE_NAME "PT100dev"

static struct i2c_adapter *adap;
static struct i2c_board_info board_info __initdata =
{
	I2C_BOARD_INFO(DEV_NAME, DEV_ADDR),
	.platform_data = NULL,
};
static struct i2c_client *client;

static int __init pt100dev_module_init(void)
{
	adap = i2c_get_adapter(1);

	client = i2c_new_device(adap, &board_info);

	if(!client)
	{
		PDEBUG("i2c new device failed\n");
	}
	
	PDEBUG("initialized\n");

	return 0;
}

static void __exit pt100dev_module_exit(void)
{
	i2c_del_device(client);
	i2c_put_adapter(adap);
	PDEBUG("exited\n");
}

module_init(pt100dev_module_init);
module_exit(pt100dev_module_exit);

MODULE_AUTHOR("cfn");
MODULE_DESCRIPTION("PT100 I2C device initializer");
MODULE_LICENSE("GPL");
