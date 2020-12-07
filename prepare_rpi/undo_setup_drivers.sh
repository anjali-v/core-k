INSMOD=/usr/bin/ins_mod
RMMOD=/usr/bin/rm_mod
DRIVERDIR=/lib/modules/4.14.74-rt44-v7+/kernel/drivers/

$RMMOD data_getter

$RMMOD i2c_bcm2835_hrt
$INSMOD "${DRIVERDIR}/i2c/busses/i2c-bcm2835.ko"

$RMMOD spi_bcm2835aux_hrt
$INSMOD "${DRIVERDIR}/spi/spi-bcm2835aux.ko"

$RMMOD spi_bcm2835_hrt
$INSMOD "${DRIVERDIR}/spi/spi-bcm2835.ko"
