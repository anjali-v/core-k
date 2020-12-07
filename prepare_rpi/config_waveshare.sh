echo "WARNING run this only on senseboard, not your PC!!!"

      read -p "Confirm? (y/n) " -n 1 -r
      echo

      if [[ ! $REPLY =~ ^[Yy]$ ]]
      then
              exit 1
      fi

echo "max_usb_current=1" >> /boot/config.txt
echo "hdmi_group=2" >> /boot/config.txt
echo "hdmi_mode=1" >> /boot/config.txt
echo "hdmi_mode=87" >> /boot/config.txt
echo "hdmi_cvt 1024 600 60 6 0 0 0" >> /boot/config.txt
echo "hdmi_drive=1" >> /boot/config.txt
