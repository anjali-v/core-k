echo -n "checking internet connectivity..."

ping -q -c5 google.com > /dev/null

if [ $? -eq 0 ]
then
	echo "ok"
else
	echo "No. Please fix before running.\n"
	exit
fi

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

echo "PROD  = ${PROD}"
echo "DEV   = ${DEV}"
echo "TEST  = ${TEST}"

if [ -z "$PROD" ]
then
	PROD=0
fi
if [ -z "$DEV" ]
then
	DEV=0
fi
if [ -z "$TEST" ]
then
	TEST=0
fi
echo "add user aha" 
echo -e "root\nroot" | sudo passwd root
sudo useradd -m aha -s /bin/bash
echo -e "aha\naha" | sudo passwd aha
sudo usermod -a -G aha,adm,dialout,cdrom,sudo,audio,video,plugdev,games,users,input,netdev,spi,i2c,gpio  aha

echo "changing default login user..."
sudo sed -i 's/autologin-user=pi/autologin-user=aha/g' /etc/lightdm/lightdm.conf

pcmanfm --set-wallpaper /usr/share/ahalogo.png --wallpaper-mode=fit

#start ssh
sudo systemctl enable ssh.service
echo -e "secretpassword\nsecretpassword" | sudo passwd pi


cd ~/uspace_progs
make

cd ~

sudo chown root:root /usr/bin/ins_mod
sudo chown root:root /usr/bin/rm_mod
sudo chmod 4755 /usr/bin/ins_mod
sudo chmod 4755 /usr/bin/rm_mod

#remove unwanted packages

echo -e "Y\n" | sudo apt remove libreoffice-* minecraft-pi scratch scratch2 geany idle3 sonic-pi sense-hat sense-emu-tools scratch scratch2
echo -e "Y\n" | sudo apt autoremove


#install dependencies for linuxcnc
echo -e "Y\n" | sudo xargs -a dependencies.txt apt install

echo "change hostname"
echo "ahaprime" | sudo tee /etc/hostname
sudo sed -i 's/raspberrypi/ahaprime/g' /etc/hosts

sudo su - aha
pcmanfm --set-wallpaper /usr/share/ahalogo.png --wallpaper-mode=fit

echo "Done"
