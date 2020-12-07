#!/usr/bin/python
import subprocess
import re
import logging.config
import os
import sys

LOGGING = {
        'version': 1,
        'formatters': {
            'simple': {
                "format": "%(asctime)s;%(lineno)s;%(module)s;%(funcName)s;%(threadName)s;%(levelname)s;%(message)s",
                'datefmt': "%d %b %Y %H:%M:%S"
            },
        },

        'handlers': {

            'console': {
                "class": "logging.StreamHandler",
                "level": "INFO",
                "stream": "ext://sys.stdout"
            },

            'debug_log': {
                "class": "logging.handlers.RotatingFileHandler",
                "level": "DEBUG",
                "formatter": "simple",
                "filename": os.path.join("/tmp", "debug.log")
            },
        },
        'loggers': {

            'log': {
                'level': 'INFO',
                'handlers': ['console','debug_log'],
                'propagate': False
            },
        },

        'root': {
            'level': 'DEBUG',
            'handlers': ['console','debug_log']
        }

}

logging.config.dictConfig(LOGGING)   
logger=logging.getLogger('log')

def prompt_user(strA):
    logger.info("\nIs " + str(strA) + " ok? Yes/No/Repeat: (Y/N/R):")
    inp= raw_input()
    if inp=='Y' or inp=='y' or inp=='yes' or inp=='Yes':
        logger.info (strA + ": [PASS]")
    elif inp=='N':
        logger.info (strA + ": [FAIL]")
    else:
        logger.info (strA + ": [REDO]")
        return 'R'
    return inp

def run_command(arg1,arg2):

    p = subprocess.Popen(arg1,stdin=subprocess.PIPE,stdout=subprocess.PIPE)
    result = p.communicate(arg2)

    return result 


logger.info ("""
Script of testing senseboard HWs
""" )

#subprocess.call(['undo_setup_drivers.sh'],stdout=open(os.devnull,'wb'))
subprocess.call(['undo_setup_drivers.sh'],shell=True)
logger.info ("""
""")
i2chw,x=run_command(['i2cdetect', '-y','1'],None)
logger.info("Checking i2c at Hardware Level...")
if re.search(r"2[04]",i2chw):
    if re.search(r"4[0]",i2chw):
        if re.search(r"6[8]",i2chw):
            logger.info("[PASS]")
        else:
            logger.info("[FAIL]")

logger.info ("""
""")

subprocess.call(['setup_drivers.sh'],shell=True)
logger.info ("""
""")


hrt,x=run_command(['lsmod'],None)

logger.info("Checking data_getter module...")
if re.search(r"data_getter.*0",hrt):
    logger.info ("[PASS]\n");
else:
    logger.info ("[FAIL]\n");
    exit(1)

logger.info("Checking spi0 hrtmodule...")
if re.search(r"spi_bcm2835_hrt.*data_getter",hrt):
    logger.info("[PASS]")
else:
    logger.info("[FAIL]")
logger.info("Checking spi1 hrtmodule...")
if re.search(r"spi_bcm2835aux_hrt.*data_getter",hrt):
    logger.info("[PASS]")
else:
    logger.info("[FAIL]")
logger.info("Checking i2c hrtmodule...")
if re.search(r"i2c_bcm2835_hrt.*data_getter",hrt):
    logger.info("[PASS]")
else:
    logger.info("[FAIL]")



###########STATUS PINS############
logger.info ("""
STATUS PINS
""" )

flag='R'
while flag=='R':

    statP,x=run_command(['hwtest'],'19')
    if bool(int(re.search(r"estop.*(\d+)",statP).group(1),10)):
        logger.info ("Estop HIT DETECTED")
    else:
        logger.info ("Estop UNHIT DETECTED")

    if bool(int(re.search(r"door.*(\d+)",statP).group(1),10)):
        logger.info ("DOOR latch NOT DETECTED")
    else:
        logger.info ("DOOR latch DETECTED")

    if bool(int(re.search(r"xmin_hit.*(\d+)",statP).group(1),10)):
        logger.info ("xmin sw NOT DETECTED")
    else:
        logger.info ("xmin sw DETECTED")

    if bool(int(re.search(r"xmax_hit.*(\d+)",statP).group(1),10)):
        logger.info ("xmax sw NOT DETECTED")
    else:
        logger.info ("xmax sw DETECTED")

    if bool(int(re.search(r"ymin_hit.*(\d+)",statP).group(1),10)):
        logger.info ("ymin sw NOT DETECTED")
    else:
        logger.info ("ymin sw DETECTED")

    if bool(int(re.search(r"ymax_hit.*(\d+)",statP).group(1),10)):
        logger.info ("ymax sw NOT DETECTED")
    else:
        logger.info ("ymax sw DETECTED")

    if bool(int(re.search(r"zmin_hit.*(\d+)",statP).group(1),10)):
        logger.info ("zmin sw NOT DETECTED")
    else:
        logger.info ("zmin sw DETECTED")

    if bool(int(re.search(r"zmax_hit.*(\d+)",statP).group(1),10)):
        logger.info ("zmax sw NOT DETECTED")
    else:
        logger.info ("zmax sw DETECTED")

    flag=prompt_user("Status pins test")
##############RTDs####################

logger.info ("""
RTDs
""")
flag='R'

while flag=='R':

    rtd,x=run_command(['hwtest'],'17')
    rtd1=re.search(r"temp0=(\d+\.*\d*) temp1=(\d+\.*\d*) temp2=(\d+\.*\d*) temp3=(\d+\.*\d*) temp4=(\d+\.*\d*)",rtd)
    for i in range(1,6):
        if (bool(float(rtd1.group(i)))>0 and (float(rtd1.group(i)))<=300):
            logger.info ("RTD " +str(i) +" DETECTED with value : " + str(rtd1.group(i)))
        else:
            logger.info ("RTD" +str(i) +" NOT DETECTED")
    flag=prompt_user("senseboard RTD test")

############# COUNTER  #######################

logger.info ("""
COUNTERs
""" )
cntr,x=run_command(['hwtest'],'18')
#if re.search(r"crc.*9f",cntr):
flag='R'
while flag:
    try:
        while True:
            cntr,x=run_command(['hwtest'],'18')
            print ("\r[press ^C] CHANNEL 0:" + str(re.findall(r"c0: (.\S+).*",cntr))+"CHANNEL 1:"+str(re.findall(r"c1: (.\S+).*",cntr))+"CHANNEL 2:"+str(re.findall(r"c2: (.\S+).*",cntr))+"CHANNEL 3:"+str(re.findall(r"c3: (.\S+).*",cntr))+"CHANNEL 4:"+str(re.findall(r"c4: (.\S+).*",cntr)))
            sys.stdout.write("\033[F")
    except KeyboardInterrupt:
        flag=prompt_user("counter test")

########## MOTOR ENABLE ####################


logger.info ("""
MOTOR ENABLE
""")
run_command(['hwtest'],'2\n1\n')
for i in range(0,3):
    arg="5\n"+str(i)+"\n3\n"
    run_command(['hwtest'],arg)


for i in range(0,3):
    arg="6\n"+str(i)+"\n3\n"
    run_command(['hwtest'],arg)

prompt_user("XYZ-motor enable")
flag='R'
while flag=='R':
    for i in range(0,3):
        logger.info("running"+str(i)+"motor...")
        arg="20\n"+str(i)+"\n"
        run_command(['hwtest'],arg)
        #flag=prompt_user("DID"+str(i)+"motor JOGGED accordingly... ")
    flag=prompt_user("XYZ motor")

for i in range(0,3):
    arg="6\n"+str(i)+"\n0\n"
    run_command(['hwtest'],arg)

########## RGB LEDs  ####################

logger.info ("""
RGB LEDs
""" )

testmode=run_command(['hwtest'],'24\n')
RGB=run_command(['hwtest'],'9\n0\n0\n0\n')

prompt_user( "RGB power selection Jumper")
Red=run_command(['hwtest'],'9\n100\n0\n0\n')
prompt_user( "RED led Glowing")
Green=run_command(['hwtest'],'9\n0\n100\n0\n')
prompt_user( "GREEN led Glowing")
Blue=run_command(['hwtest'],'9\n0\n0\n100\n')
prompt_user( "BLUE led Glowing")
RGB=run_command(['hwtest'],'9\n0\n0\n0\n')


########## HEATERs ####################


logger.info ("""
HEATERSs
""" )

prompt_user( "HEATERS power selection Jumper")
Bed=run_command(['hwtest'],'7\n4095\n0\n0\n0\n')
prompt_user( "BED heater working")
Chamber_L=run_command(['hwtest'],'7\n0\n4095\n0\n0\n')
prompt_user( "CHAMBER_LEFT heater working")
Chamber_R=run_command(['hwtest'],'7\n0\n0\n4095\n0\n')
prompt_user( "CHAMBER_RIGHT heater working")
Baker=run_command(['hwtest'],'7\n0\n0\n0\n4095\n')
prompt_user( "BAKER heater working")
heater=run_command(['hwtest'],'7\n0\n0\n0\n0\n')


########## HOOTER ####################


logger.info("""
HOOTER EN
""" )

Hooter=run_command(['hwtest'],'8\n1\n')
prompt_user( "HOOTER working")
Hooter=run_command(['hwtest'],'8\n0\n')


########## HEAD Enable  ####################


logger.info ("""
Enable 220v
""" )

en_220=run_command(['hwtest'],'11\n1\n')
prompt_user( "Enable 220v working")
en_220=run_command(['hwtest'],'11\n0\n')


########## DOOR LOCK  ####################


logger.info ("""
Door Lock
""" )

door=run_command(['hwtest'],'10\n0\n1\n')
prompt_user( "Door Lock working")
door=run_command(['hwtest'],'10\n0\n0\n')


########## HEAD PCB  ####################


logger.info ("""
Serial
""")
flag='R'
while flag=='R':
    ser_R,x=run_command(['hwtest'],'21\n')
    ser_R1=re.findall(r"RTD A1.* (\d+\.*\d*)",ser_R)
    ser_R2=re.findall(r"RTD A2.* (\d+\.*\d*)",ser_R)
    ser_R3=re.findall(r"RTD B1.* (\d+\.*\d*)",ser_R)
    ser_R4=re.findall(r"RTD B2.* (\d+\.*\d*)",ser_R)
    ser_P12=re.findall(r"Filament Pressure A.* (\d+\.*\d*)",ser_R)
    logger.info ("RTDA1   "+str(ser_R1))
    logger.info ("RTDA2   "+str(ser_R2))
    logger.info ("RTDB1   "+str(ser_R3))
    logger.info ("RTDB2   "+str(ser_R4))
    logger.info ("Pressure"+str(ser_P12))
    flag=prompt_user( "Head PCB RTD reading")

#while True:
logger.info("HEAD motor enable")
ser_W,x=run_command(['hwtest'],'22\nA\n0\n1\n6\n')
ser_W,x=run_command(['hwtest'],'22\nB\n0\n1\n6\n')
prompt_user( "AB motors enabled")
flag='R'
while flag=='R':
    logger.info ("[PASS]\n Jogging AB axis motors....")
    motZ2=run_command(['hwtest'],'20\n3\n')
    motZ2=run_command(['hwtest'],'20\n4\n')
    flag=prompt_user( "DID AB motors jogged accordingly...")

logger.info("""
HEAD HEATERS
""")
flag='R'
while flag:
    try:
        while True:
            ser_W,x=run_command(['hwtest'],'22\nA\n255\n0\n0\n')
            ser_W,x=run_command(['hwtest'],'22\nB\n255\n0\n0\n')
            ser_R,x=run_command(['hwtest'],'21\n')
            ser_R1=re.findall(r"RTD A1.* (\d+\.*\d*)",ser_R)
            ser_R2=re.findall(r"RTD A2.* (\d+\.*\d*)",ser_R)
            ser_R3=re.findall(r"RTD B1.* (\d+\.*\d*)",ser_R)
            ser_R4=re.findall(r"RTD B2.* (\d+\.*\d*)",ser_R)
            print("\r BE Cautious Heaters are ON..!!    [press ^C]  RTD A1"+str(ser_R1)+"RTD A2"+str(ser_R2)+"RTD B1"+str(ser_R3)+"RTD B2"+str(ser_R4))
            sys.stdout.write("\033[F")
    except KeyboardInterrupt:
        flag=prompt_user( "HEAD Heaters working")
