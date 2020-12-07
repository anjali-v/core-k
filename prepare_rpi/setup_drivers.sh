INSMOD=/usr/bin/ins_mod
RMMOD=/usr/bin/rm_mod
DRIVERDIR=/lib/modules/4.19.71-rt24-v7l+/kernel/drivers/

$RMMOD i2c_bcm2835
$INSMOD ${DRIVERDIR}/i2c/busses/i2c-bcm2835-hrt.ko

$RMMOD  spi_bcm2835aux
$INSMOD ${DRIVERDIR}/spi/spi-bcm2835aux-hrt.ko

$RMMOD spi_bcm2835
$INSMOD ${DRIVERDIR}/spi/spi-bcm2835-hrt.ko

$INSMOD /etc/lena/data_getter.ko
