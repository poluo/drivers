

int setup_spi(int channel, int speed, int mode);

int do_msg(int channel, unsigned char *data, int len);

int do_read(int channel, unsigned char *data, int len);

int release_spi(int channel);
