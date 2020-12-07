echo -n "checking internet connectivity..."

ping -q -c5 google.com > /dev/null

if [ $? -eq 0 ]
then
	echo "ok"
else
	echo "No. Please fix before running.\n"
	exit
fi

#sudo apt-clone restore apt-clone-state-ahaprime.tar.gz
#sudo usermod -a -G dialout aha
#sudo systemctl enable ssh.service
echo "copying board script to root directories"
#cd files
echo "inside directory " $PWD
#make
#sudo cp gitversion_info /etc/ahaVersion
#sudo cp Launch3d.desktop /home/aha/Desktop
#sudo chmod +x /home/aha/Desktop/Launch3d.desktop
gsettings set org.gnome.desktop.background primary-color white
gsettings set org.gnome.desktop.background picture-uri file:///home/anjali/prepare_rpi/ahalogo.png
gsettings set org.gnome.desktop.background picture-options 'scaled'

echo "Done Now restart the PC"
