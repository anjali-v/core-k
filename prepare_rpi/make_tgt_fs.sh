#!/bin/bash

linuxdir=/home/anjali/rpi-kernel/linux
preptgtdir=/home/anjali/prepare_rpi
linuxahadir=$preptgtdir/linux_aha
uspacedir=$preptgtdir/uspace_progs

echo "usage ./make_tgt_fs.sh -p=2 -d=1 -t=1
Refer to https://docs.google.com/spreadsheets/d/1pHFov6wud3SUa-zKZNWOcafZ-Y-bJQ_Y3FKRBlmJ-yk/edit#gid=832935219"
for i in "$@"
do
	case $i in
		-p=*|--prod=*)
			PROD="${i#*=}"
			shift # past argument=value
			;;
		-d=*|--debug=*)
			DEV="${i#*=}"
			shift # past argument=value
			;;
		-t=*|--test=*)
			TEST="${i#*=}"
			shift # past argument=value
			;;
		--default)
			DEFAULT=YES
			shift # past argument with no value
			;;
		*)
			# unknown option
			;;
	esac
done

echo "PROD    = ${PROD} Download the deb from following link https://drive.google.com/open?id=1k24R86IoMW45pwwNPsq6CVY66RveurCd "
echo "DEV  = ${DEV}"
echo "TEST  = ${TEST}"

if [ -z "$DEV" ]
then
	DEV=0
fi
if [ -z "$TEST" ]
then
	TEST=0
fi
if [ -z "$PROD" ]
then
	PROD=0
fi

echo "use etcher to put raspbian image before you run this script."

dir=$PWD
if [[ ! $dir =~ $preptgtdir ]]
then
	echo "dir is $dir. Not $preptgtdir ! your host tree is bad! Go read the doc. Bye"
	exit 1
fi


sdrootpath=`df | grep media | grep rootfs | awk 'NF>1{print $NF}'`
sdbootpath=`df | grep media | grep boot | awk 'NF>1{print $NF}'`

if test -z "$sdrootpath" 
then
	echo "no raspbian SD card root mounted. Bye"
	exit 1
else
	echo "\$sdrootpath is " $sdrootpath
	read -p "is it ok to proceed? (y/n) " -n 1 -r
	echo

	if [[ ! $REPLY =~ ^[Yy]$ ]]
	then
		exit 1
	fi
fi


if test -z "$sdbootpath" 
then
	echo "no raspbian SD card root mounted. Bye"
	exit 1
else
	echo "\$sdbootpath is " $sdbootpath
	read -p "is it ok to proceed? (y/n) " -n 1 -r
	echo

	if [[ ! $REPLY =~ ^[Yy]$ ]]
	then
		exit 1
	fi
fi

#unpack raspbian onto SD card


#if [ "$ROUND" -eq 1 ]
#then
echo "installing Linux and configuring system"

#copy linux image on target
cd $linuxdir
dir=$PWD
if [[ ! $dir =~ $linuxdir ]]
then
	echo "dir is $dir. Not $linuxdir! your host tree is bad! Go read the doc. Bye"
	exit 1
fi

if [ ! -f arch/arm/boot/zImage ]
then
	echo "compiled kernel image absent in linux folder! Bye."
	exit 1
fi

echo "inside directory " $PWD
sudo make -j4 ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- INSTALL_MOD_PATH=$sdrootpath modules_install
if [ "$DEV" -eq 1 ]
then
	sudo make -j5 ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- INSTALL_MOD_PATH=$sdrootpath headers_install
fi


KERNEL=kernel7l
sudo cp $sdbootpath/$KERNEL.img $sdbootpath/$KERNEL-backup.img
sudo cp arch/arm/boot/zImage $sdbootpath/$KERNEL.img
sudo cp arch/arm/boot/dts/*.dtb $sdbootpath/
sudo cp arch/arm/boot/dts/overlays/*.dtb* $sdbootpath/overlays/
sudo cp arch/arm/boot/dts/overlays/README $sdbootpath/overlays/

#configuring stuff on target
sudo sed -i 's/XKBLAYOUT="gb"/XKBLAYOUT="us"/g' $sdrootpath/etc/default/keyboard
sudo cp $sdrootpath/usr/share/zoneinfo/Asia/Kolkata $sdrootpath/etc/localtime
echo "disable_splash=1" >> $sdbootpath/config.txt
echo "dtoverlay=pi3-disable-bt" >> $sdbootpath/config.txt
echo "dtoverlay=spi1-1cs36"  >> $sdbootpath/config.txt
echo "enable_uart=1" >> $sdbootpath/config.txt
echo "#kernel=kernel7.img" >> $sdbootpath/config.txt
echo "#initramfs initrd7.img" >> $sdbootpath/config.txt
echo "#max_usb_current=1" >> $sdbootpath/config.txt
echo "#hdmi_group=2" >> $sdbootpath/config.txt
echo "#hdmi_mode=1" >> $sdbootpath/config.txt
echo "#hdmi_mode=87" >> $sdbootpath/config.txt
echo "#hdmi_cvt 1024 600 60 6 0 0 0" >> $sdbootpath/config.txt
echo "#hdmi_drive=1" >> $sdbootpath/config.txt

sed -i 's/#dtparam=spi=on/dtparam=spi=on/g' $sdbootpath/config.txt
sed -i 's/#dtparam=i2c_arm=on/dtparam=i2c_arm=on/g' $sdbootpath/config.txt

sed -i 's/console=serial0,115200//g' $sdbootpath/cmdline.txt
sed -i 's/console=tty1/console=tty3/g' $sdbootpath/cmdline.txt
sed -i 's/$/ logo.nologo vt.global_cursor_default=0/g' $sdbootpath/cmdline.txt
sed -i 's/$/ boot=overlay logo.nologo vt.global_cursor_default=0 nohz=off/g' $sdbootpath/cmdline.txt

echo "copying customized files..."
cd $preptgtdir
echo "inside directory " $PWD

sudo cp spi1-1cs36.dtbo $sdbootpath/overlays/

sudo cp ahalogo.png $sdrootpath/usr/share
sudo cp ahalogo.png $sdrootpath/usr/share/plymouth/themes/pix/splash.png
sudo cp raspi-blacklist.conf $sdrootpath/etc/modprobe.d/raspi-blacklist.conf
sudo cp setup_drivers.sh $sdrootpath/usr/bin
sudo cp undo_setup_drivers.sh $sdrootpath/usr/bin


cd ..
echo "inside directory " $PWD
sudo cp -r $uspacedir $sdrootpath/home/pi

sudo cp runme_on_tgt.sh $sdrootpath/pi
sudo chmod 4755 $sdrootpath/pi/runme_on_tgt.sh
#sudo cp config_waveshare.sh $sdhomepath/pi

echo "going to linux_aha and uspace dirs, make them and copy to target."
cd $linuxahadir
echo "inside directory " $PWD
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-  LINUXPATH=$linuxdir
sudo mkdir -p $sdrootpath/etc/lena
sudo cp data_getter.ko $sdrootpath/etc/lena
sudo cp data_getter.h $sdrootpath/usr/include


sudo umount $sdbootpath
sudo umount $sdrootpath

#todo 
#copy lena files to target
