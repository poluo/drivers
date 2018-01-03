#ifndef _MAIN_H
#define _MAIN_H

#define ENABLE_DEBUG
#define PRINTK_LEVEL KERN_INFO
#define MODULE_NAME	"ioctl"
#define DEVICE_COUNT (1)
//#define S3C2440		 (1)
#undef PDEBUG
#ifdef ENABLE_DEBUG
#  ifdef __KERNEL__
#    define PDEBUG(format, args...) printk(PRINTK_LEVEL MODULE_NAME ": " format, ##args)
#  else
#    define PDEBUG(format, args...) fprintf(stderr, MODULE_NAME ": " format, ##args)
#  endif
#else
#  define PDEBUG(format, args...)
#endif

#endif
