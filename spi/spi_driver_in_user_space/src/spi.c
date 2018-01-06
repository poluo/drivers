/* 
BASIC CHARACTER DEVICE API
==========================
Normal open() and close() operations on /dev/spidevB.D files work as you
would expect.

Standard read() and write() operations are obviously only half-duplex, and
the chipselect is deactivated between those operations.  Full-duplex access,
and composite operation without chipselect de-activation, is available using
the SPI_IOC_MESSAGE(N) request.

Several ioctl() requests let your driver read or override the device's current
settings for data transfer parameters:

    SPI_IOC_RD_MODE, SPI_IOC_WR_MODE ... pass a pointer to a byte which will
    return (RD) or assign (WR) the SPI transfer mode.  Use the constants
    SPI_MODE_0..SPI_MODE_3; or if you prefer you can combine SPI_CPOL
    (clock polarity, idle high iff this is set) or SPI_CPHA (clock phase,
    sample on trailing edge iff this is set) flags.
    Note that this request is limited to SPI mode flags that fit in a
    single byte.

    SPI_IOC_RD_MODE32, SPI_IOC_WR_MODE32 ... pass a pointer to a uin32_t
    which will return (RD) or assign (WR) the full SPI transfer mode,
    not limited to the bits that fit in one byte.

    SPI_IOC_RD_LSB_FIRST, SPI_IOC_WR_LSB_FIRST ... pass a pointer to a byte
    which will return (RD) or assign (WR) the bit justification used to
    transfer SPI words.  Zero indicates MSB-first; other values indicate
    the less common LSB-first encoding.  In both cases the specified value
    is right-justified in each word, so that unused (TX) or undefined (RX)
    bits are in the MSBs.


    SPI_IOC_RD_BITS_PER_WORD, SPI_IOC_WR_BITS_PER_WORD ... pass a pointer to
    a byte which will return (RD) or assign (WR) the number of bits in
    each SPI transfer word.  The value zero signifies eight bits.

    SPI_IOC_RD_MAX_SPEED_HZ, SPI_IOC_WR_MAX_SPEED_HZ ... pass a pointer to a
    u32 which will return (RD) or assign (WR) the maximum SPI transfer
    speed, in Hz.  The controller can't necessarily assign that specific
    clock speed.

*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <asm/ioctl.h>
#include <linux/spi/spidev.h>


static const char       *spiDev0  = "/dev/spidev0.0" ;
static const char       *spiDev1  = "/dev/spidev0.1" ;
static const uint8_t     spiBPW   = 8 ;
static const uint16_t    spiDelay = 0 ;

static uint32_t    spiSpeeds[2] ;
static int         spiFds[2] ;
// use read() api
int do_read(int channel, unsigned char *data, int len)
{
    int     status;

    status = read(spiFds[channel], data, len);
    if (status < 0) 
    {
        printf("read");
        return status;
    }
    if (status != len) 
    {
        fprintf(stderr, "short read\n");
    }
    return status;
}
// use SPI_IOC_MESSAGE
int do_msg(int channel, unsigned char *data, int len)
{
    struct spi_ioc_transfer spi ;

    channel &= 1 ;

    // Mentioned in spidev.h but not used in the original kernel documentation
    //  test program )-:

    memset (&spi, 0, sizeof (spi)) ;

    spi.tx_buf        = (unsigned long)data ;
    spi.rx_buf        = (unsigned long)data ;
    spi.len           = len ;
    spi.delay_usecs   = spiDelay ;
    spi.speed_hz      = spiSpeeds[channel] ;
    spi.bits_per_word = spiBPW ;
    return ioctl(spiFds[channel], SPI_IOC_MESSAGE(1), &spi);
}
int setup_spi(int channel, int speed, int mode)
{
    int fd ;
    int success = 0;

    mode    &= 3 ;    // Mode is 0, 1, 2 or 3
    channel &= 1 ;    // Channel is 0 or 1

    if ((fd = open(channel == 0 ? spiDev0 : spiDev1, O_RDWR)) < 0)
    {
        success = -1;
        printf("Unable to open SPI device: %s\n", strerror (errno));
        goto end;
    }

    spiSpeeds[channel] = speed ;
    spiFds[channel] = fd ;

    // Set SPI parameters.

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0)
    {
        success = -1;
        printf("SPI Mode Change failure: %s\n", strerror(errno));
        goto err;

    }    
  
    if(ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW) < 0)
    {
        success = -1;
        printf("SPI BPW Change failure: %s\n", strerror(errno));
        goto err;
    }

    if(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed)   < 0)
    {
        success = -1;
        printf("SPI Speed Change failure: %s\n", strerror(errno));
        goto err;
    }
err:
    if(success != 0)
    {
        close(fd);
    }
end:
    return success ;
}

int release_spi(int channel)
{
    return close(spiFds[channel]);
}