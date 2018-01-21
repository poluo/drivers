
#define MODULE_NAME	"all"
#define PDEBUG(format, args...) printk(KERN_INFO MODULE_NAME ": " format, ##args)

extern void test_timer(int data);

extern void test_sleep_method(int to_test);

extern void print_time(void);