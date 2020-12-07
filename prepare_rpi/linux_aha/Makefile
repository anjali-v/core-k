obj-m += hello.o data_getter.o proc_rw.o spi-bcm2835-hrt.o i2c-bcm2835-hrt.o spi-bcm2835aux-hrt.o 

MY_CFLAGS += -g -DDEBUG
ccflags-y += ${MY_CFLAGS}
CC += ${MY_CFLAGS}

all:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C ${LINUXPATH} M=$(PWD) modules
clean:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- M=$(PWD) clean
