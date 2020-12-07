# Prepare RPI
This repo main objective is to build the card for *Senseboard-RPI* with all the RT-functionalities,modules,copying the necessary root files or editing them.


## Pre-configuration steps in card

1) Use the image of [RPI](https://www.raspberrypi.org/downloads/raspbian/) to install rasbpian using [etcher](https://www.balena.io/etcher/) software.

2) Use  **gparted**  for making the partion in card with following specifications :
   * Make a new partion in the freed space named as **home** with 5GB space and ext4 format.
   * Resize /root partition to 27GB.

3) After re-mounting of card edit the PARTUID of all 3 partition(boot+root+home) in `/boot/cmdline.txt` and `/rootfs/etc/fstab` .
4) Copy the `/rootfs/home/pi` directory to `/home/` and change owner privlages to $USER
        
        cd /home/$USER/media/home/
        sudo chown -R $USER:$USER /pi
5) Boot the card in rpi.


## USAGE
1) Use the card making script [make_tgt_fs](https://gitlab.com/aha3d/prepare_rpi/blob/master/make_tgt_fs.sh) after raspian install and partition making,this script auto build the rt-stuffs,edit the config and cmdline files,copy necessay files to their destination as per the requirement.

2) Download these from [Gdrive](https://drive.google.com/drive/folders/1nMK5ZINavGZKH4QQVzmvsupTysRUW41a) before using the script. 
`apt-clone-state-ahaprime.tar.gz`
`linuxcnc-uspace-dev_2.7.12_armhf.deb`
`linuxcnc-uspace_2.7.12_armhf.deb`


```bash
        ./make_tgt_fs.sh -p=1   <for production>
        ./make_tgt_fs.sh -d=1   <for development>
        ./make_tgt_fs.sh -t=1   <for testing of hw>
```
3) Now again boot the card in rpi.

4) Make sure to connect rpi with internet and run [runme_on_tgt](https://gitlab.com/aha3d/prepare_rpi/blob/master/runme_on_tgt.sh), this will create the new host name `aha`and install+purge all the dependencies and packages.
```bash
        ./runme_on_tgt.sh -p=1   <for production>
        ./runme_on_tgt.sh -d=1   <for development>
        ./runme_on_tgt.sh -t=1   <for testing of hw>
```
5) Reboot

## Useful Links

1. [TasksheetRPI](https://docs.google.com/spreadsheets/d/1MOnoOCUmxuk3Mf7MBbmNZCapTWjdg9ol69WFpsA5ig4/edit#gid=1771380656)
2. [BringUp Sheet](https://docs.google.com/spreadsheets/d/1pHFov6wud3SUa-zKZNWOcafZ-Y-bJQ_Y3FKRBlmJ-yk/edit#gid=0)
3. [Software bringUP](https://docs.google.com/document/d/1tfPwbHUQCr0NCNBjDOYZ9dceTmpI2gg0JaUTyMlp89I/edit?usp=drive_web&ouid=117754949105562408248)
