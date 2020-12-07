/*
 * When disabled: read all values anyway, but set all outputs to zero or to safe values
 * When internal error detected: -do-
 * When enabled: okay to set values as per user wants
 */

//#define OLD_SENSEBOARD

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>  // for threads
#include <linux/sched.h>  // for task_struct
#include <linux/time.h>   // for using jiffies 
#include <linux/timer.h>
#include <linux/sched/signal.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include "data_getter.h"

#define MODNAME "data_getter"
#ifdef OLD_SENSEBOARD
#warning "Compiling for OLD_SENSEBOARD!"
#define RGB_HW_ERROR    {}
#define RGB_HW_DISABLED {}
#define RGB_HW_ENABLED  {}
#else
#define RGB_HW_ERROR    {if(!test)set_rgb (RED, LIGHT_BLINK);}
#define RGB_HW_DISABLED {if(!test)set_rgb (CYAN, LIGHT_ON);}
#define RGB_HW_ENABLED  {if(!test)set_rgb (WHITE, LIGHT_ON);}
#endif
//////// GLOBAL DATA ////////////
testmode_t test=0;
stats_t module_stats;
iostatus_t iostatus;
rgbdata_t rgb;
spinlock_t rgb_lock;
spinlock_t i2c_lock;
bool gCannot_continue; 

struct proc_dir_entry *parent=NULL;

char rxbuf[10], txbuf[10], init_buf[2];

static struct hrtimer fast_timer;
static struct hrtimer slow_timer;
static struct hrtimer sloow_timer;
spinlock_t fast_timer_callback_lock;
spinlock_t slow_timer_callback_lock;
spinlock_t sloow_timer_callback_lock;

#define US_100    100000
#define US_200    200000
#define US_500    500000
#define US_600    600000
#define MS_1     1000000
#define MS_5     5000000
#define MS_10   10000000
#define MS_100 100000000  
#define MS_500 500000000 
#define S_1   1000000000

#define MILLIS(val) val/MS_1

unsigned long small_delay = MS_1, large_delay=MS_10, laarge_delay=MS_100, laarge_delay_ms = 100, tprof=1; 

//char rtdbuf[NUM_RTDS][RTD_XFERLEN] rx_rtdbuf; 
rtdbuf_t rx_rtdbuf;

// SPI 1 related
spibuf_t old_rx_ctrbuf, rx_ctrbuf, tx_ctrbuf;

struct spi_master *pMaster0=NULL;
struct spi_device *pSpi0=NULL;
struct spi_master *pMaster1=NULL;
struct spi_device *pSpi1=NULL;
struct i2c_adapter *pAdap=NULL;

struct i2c_msg i2cmsg[2];
char buf0[2], buf1[2];

struct portval 
{
	spinlock_t lock;
	bool changed;
	bitfields_t u;
} gpiox1b_reg, gpiox2a_reg, gpiox2b_reg;

struct pwmval
{
	spinlock_t lock;
	uint8_t channel;
	uint16_t wanted_value;
	uint16_t actual_value;
} pwm_channel[NUM_PWM_CHANNELS];

uint8_t val8;

//////// EXTERNS //////////////////
extern int bcm2835aux_spi_prepare_message (struct spi_master *, struct spi_message *);
extern int bcm2835aux_spi_transfer_one (struct spi_master *, struct spi_device *, struct spi_transfer *);
extern int bcm2835aux_spi_get_master(struct spi_master **);
extern int bcm2835aux_spi_get_spi(struct spi_device **);

extern int bcm2835_spi_prepare_message (struct spi_master *, struct spi_message *);
extern int bcm2835_spi_transfer_one (struct spi_master *, struct spi_device *, struct spi_transfer *);
extern int bcm2835_spi_get_master(struct spi_master **);
extern int bcm2835_spi_get_spi(struct spi_device **);

extern int bcm2835_i2c_get_adapter(struct i2c_adapter **);
extern int bcm2835_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num);

///////////// PROC ENTRIES ///////////////////
/*
   /proc/senseos

encoders  : read  : refreshed 50us reading it will return most recent counter values, corrected by checksum
rtd0..4   : read  : refreshed 10ms
heater0..4: write : on every write of proc entry
hooter    :  -do- :    - do - 
led_rgb   :  -do- :    - do -
*/

// RTDs
int proc_get_rtd (struct file *filp,char *buf,size_t count,loff_t *offp ) 
{
	/*	if((int)*offp > 0)
		{
		return 0;
		}
		*/
	//	printk ("%s: in %s\n", MODNAME, __FUNCTION__);
	if (copy_to_user(buf, &rx_rtdbuf.buf, sizeof(rx_rtdbuf)) != 0)
		printk ("%s copy_to_user failed!\n", MODNAME);	

	/*	for (n=0; n<NUM_RTDS;n++)
		{
		printk (KERN_INFO "rx: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", 
		rx_rtdbuf.buf[n][0],
		rx_rtdbuf.buf[n][1],
		rx_rtdbuf.buf[n][2],
		rx_rtdbuf.buf[n][3],
		rx_rtdbuf.buf[n][4],
		rx_rtdbuf.buf[n][5],
		rx_rtdbuf.buf[n][6],
		rx_rtdbuf.buf[n][7],
		rx_rtdbuf.buf[n][8]);
		}
		printk ("\n");
		*/
	*offp += sizeof(rx_rtdbuf);
	return sizeof(rx_rtdbuf);
}

struct file_operations proc_rtd_fops = {
read: proc_get_rtd,
};

int proc_get_counters (struct file *filp, char *buf, size_t count, loff_t *offp ) 
{
	/*	if((int)*offp > 0)
		{
		return 0;
		}*/

	//	printk ("%s: in %s\n", MODNAME, __FUNCTION__);
	if (copy_to_user(buf, &rx_ctrbuf, sizeof(rx_ctrbuf)) != 0)
		printk ("%s copy_to_user failed!\n", MODNAME);	
	/*	printk (KERN_INFO "%s: %s - %02x %08x %08x %08x %08x %08x\n", MODNAME, __FUNCTION__,
		rx_ctrbuf.cksum, 
		rx_ctrbuf.counter[0].data32, 
		rx_ctrbuf.counter[1].data32, 
		rx_ctrbuf.counter[2].data32, 
		rx_ctrbuf.counter[3].data32,
		rx_ctrbuf.counter[4].data32);
		*/
	*offp += sizeof(rx_ctrbuf);
	return sizeof(rx_ctrbuf);
}

struct file_operations proc_counters_fops = {
read: proc_get_counters,
};

// PWM
void schedule_set_pwm(int channel, int value)
{
	if (pwm_channel[channel].actual_value != pwm_channel[channel].wanted_value) module_stats.pwm_update_missed++;

	spin_lock(&(pwm_channel[channel].lock));
	pwm_channel[channel].wanted_value = value; 
	spin_unlock(&(pwm_channel[channel].lock));
	//printk (KERN_INFO "%s: %s setting chan %d pwm %d\n", MODNAME , __FUNCTION__, channel, value);
}

int write_i2c(uint8_t address, uint8_t reg, uint8_t data)
{
	buf0[0] = reg;
	buf0[1] = data;
	printk (KERN_INFO "%s: %s chip %02x setting data 0x%02x 0x%02x\n", MODNAME , __FUNCTION__, address, buf0[0], buf0[1]);

	i2cmsg[0].addr = address;
	i2cmsg[0].flags = 0;
	i2cmsg[0].len = 2;
	i2cmsg[0].buf = buf0;

	if (pAdap) {
		if (bcm2835_i2c_xfer(pAdap, i2cmsg, 1) < 0) {
			module_stats.i2c_errors++;
			return -1;
		}
		else
			module_stats.i2c_transfers++;
	}
	else return -1;
	return 0;
}

int read_i2c(uint8_t address, uint8_t reg, uint8_t *data)
{	
	//printk(KERN_INFO "%s: in %s", MODNAME, __FUNCTION__);
	buf0[0]=reg;

	i2cmsg[0].addr = address;
	i2cmsg[0].flags = 0;
	i2cmsg[0].len = 1;
	i2cmsg[0].buf = buf0;

	i2cmsg[1].addr = address;
	i2cmsg[1].flags = I2C_M_RD;
	i2cmsg[1].len = 1;
	i2cmsg[1].buf = buf1;


	if (pAdap) 
	{
		if (bcm2835_i2c_xfer(pAdap, i2cmsg, 2) < 0) {
			module_stats.i2c_errors++;
			return -1; //just bail out dont update the data with your crap buf1!
		}
		else 
			module_stats.i2c_transfers++;
	}
	else
	{
		module_stats.i2c_errors++;
		return -1;
	}

	*data = buf1[0];
	return 0;
}

int init_pwm_chip (void)
{
	uint8_t mode;
	printk (KERN_INFO "%s: in %s\n", MODNAME , __FUNCTION__);
	if (write_i2c (PCA9685_ADDRESS, MODE2, PWM_OUTDRV) != 0) {
		printk (KERN_ERR "%s: %s error writing to i2c.\n", MODNAME , __FUNCTION__); return -1;}

	if (write_i2c (PCA9685_ADDRESS, MODE1, PWM_ALLCALL) != 0) {
		printk (KERN_ERR "%s: %s error writing to i2c.\n", MODNAME , __FUNCTION__); return -1;}
	mdelay (5);

	if (read_i2c(PCA9685_ADDRESS, MODE1, &mode) != 0) {
		printk (KERN_ERR "%s: %s error reading mode1.\n", MODNAME , __FUNCTION__); return -1;}
	mode = mode & ~PWM_SLEEP;

	if (write_i2c (PCA9685_ADDRESS, MODE1, mode) != 0) {
		printk (KERN_ERR "%s: %s error writing to i2c.\n", MODNAME , __FUNCTION__); return -1;}
	mdelay (5);
	return 0;
}

int set_pwm_freq (void)
{
	uint8_t prescale, oldmode, newmode;

	printk (KERN_INFO "%s: in %s\n", MODNAME , __FUNCTION__);
	prescale = 101; //for 60 Hz pwm
	if (read_i2c(PCA9685_ADDRESS, MODE1, &oldmode) != 0) {
		printk (KERN_ERR "%s: %s error reading mode1.\n", MODNAME , __FUNCTION__); return -1;}

	newmode = (oldmode & 0x7f) | 0x10;

	//now write newmode, prescale, oldmode, oldmode|0x80
	if (write_i2c (PCA9685_ADDRESS, MODE1, newmode) != 0) {
		printk (KERN_ERR "%s: %s error writing to i2c mode1.\n", MODNAME , __FUNCTION__); return -1;}

	if (write_i2c (PCA9685_ADDRESS, PRESCALE, prescale) != 0) {
		printk (KERN_ERR "%s: %s error writing to i2c prescale.\n", MODNAME , __FUNCTION__); return -1;}

	if (write_i2c (PCA9685_ADDRESS, MODE1, oldmode) != 0) {
		printk (KERN_ERR "%s: %s error writing to i2c mode1.\n", MODNAME , __FUNCTION__); return -1;}

	mdelay (5);
	if (write_i2c (PCA9685_ADDRESS, MODE1, oldmode|0x80) != 0) {
		printk (KERN_ERR "%s: %s error writing to i2c mode1.\n", MODNAME , __FUNCTION__); return -1;}

	return 0;
}

int set_pwm(int channel)
{
	int on=0, off;
	if (pwm_channel[channel].actual_value == pwm_channel[channel].wanted_value) return 0;

	off = pwm_channel[channel].wanted_value;

	//printk (KERN_INFO "%s: %s channel %d value %d\n", MODNAME , __FUNCTION__, channel, off);
	buf0[0]=LED0_ON_L+4*channel; buf0[1]= (on & 0xff);
	//printk (KERN_INFO "%s: %s will set 0x%02x: 0x%02x 0x%02x\n", MODNAME , __FUNCTION__, PWM_CHIP, buf0[0], buf0[1]);

	i2cmsg[0].addr = PWM_CHIP;
	i2cmsg[0].flags = 0;
	i2cmsg[0].len = 2;
	i2cmsg[0].buf = buf0;

	if (pAdap) {
		if (bcm2835_i2c_xfer(pAdap, i2cmsg, 1) < 0) 
			module_stats.i2c_errors++;
		else
			module_stats.i2c_transfers++;
	}
	else return -1;

	buf0[0]=LED0_ON_H+4*channel; buf0[1]= (on >> 8);
//	printk (KERN_INFO "%s: %s will set 0x%02x 0x%02x\n", MODNAME , __FUNCTION__, buf0[0], buf0[1]);

	i2cmsg[0].addr = PWM_CHIP;
	i2cmsg[0].flags = 0;
	i2cmsg[0].len = 2;
	i2cmsg[0].buf = buf0;

	if (pAdap)  {
		if (bcm2835_i2c_xfer(pAdap, i2cmsg, 1) < 0) 
			module_stats.i2c_errors++;
		else
			module_stats.i2c_transfers++;
	}
	else return -1;

	buf0[0]=LED0_OFF_L+4*channel; buf0[1]= (off & 0xff);
//	printk (KERN_INFO "%s: %s will set 0x%02x 0x%02x\n", MODNAME , __FUNCTION__, buf0[0], buf0[1]);

	i2cmsg[0].addr = PWM_CHIP;
	i2cmsg[0].flags = 0;
	i2cmsg[0].len = 2;
	i2cmsg[0].buf = buf0;

	if (pAdap)  {if (bcm2835_i2c_xfer(pAdap, i2cmsg, 1) < 0) module_stats.i2c_errors++; 
		else module_stats.i2c_transfers++; }
	else return -1;

	buf0[0]=LED0_OFF_H+4*channel; buf0[1]= (off >> 8);
//	printk (KERN_INFO "%s: %s will set 0x%02x 0x%02x\n", MODNAME , __FUNCTION__, buf0[0], buf0[1]);

	i2cmsg[0].addr = PWM_CHIP;
	i2cmsg[0].flags = 0;
	i2cmsg[0].len = 2;
	i2cmsg[0].buf = buf0;

	if (pAdap)  {if (bcm2835_i2c_xfer(pAdap, i2cmsg, 1) < 0) module_stats.i2c_errors++; 
		else module_stats.i2c_transfers++; }
	else return -1;

	spin_lock(&(pwm_channel[channel].lock));
	pwm_channel[channel].actual_value = off; 
	spin_unlock(&(pwm_channel[channel].lock));

	return 0;
}

void turn_off_all_heaters(void)
{
	int i;
	for (i=HEATER_INDEX; i<NUM_HEATERS; i++)
		schedule_set_pwm (i, 0);
}

void turn_off_all_pwm_channels(void)
{
	int i;
	for (i=0; i<NUM_PWM_CHANNELS; i++)
		schedule_set_pwm (i, 0);
}

int proc_set_heater (struct file *filp,const char *buf,size_t count,loff_t *offp)
{
	heaterdata_t data;
	int i,j;
	//printk (KERN_ERR "%s: in %s \n", MODNAME, __FUNCTION__);
	if (count != sizeof(heaterdata_t))
		printk (KERN_ERR "%s: %s - what the?! %d %d \n", MODNAME, __FUNCTION__, count, sizeof(heater_t));
	if (copy_from_user (&data, buf, count) != 0) module_stats.copy_from_user_errors++;
	for (i=HEATER_INDEX, j=0; j < NUM_HEATERS; i++,j++)
	{
		if ((data.value[j] < 0) || (data.value[j] > 4095)) 
		{
			printk (KERN_ERR "%s: %s bad data %d \n", MODNAME, __FUNCTION__, data.value[j]);
			return -1;
		}
		schedule_set_pwm (i, data.value[j]);
	}
	return count;
}

int proc_set_rgb (struct file *filp,const char *buf,size_t count,loff_t *offp)
{
	rgbdata_t data;
	printk ("%s: in %s\n", MODNAME, __FUNCTION__);
	if (count != sizeof(rgbdata_t))
		printk (KERN_ERR "%s: %s - what the?! %d %d \n", MODNAME, __FUNCTION__, count, sizeof(rgbdata_t));
	if (copy_from_user (&data, buf, count) != 0) 
	{
		module_stats.copy_from_user_errors++;
		return -1;
	}

	if ((data.red < PWM_MAX) && (data.green < PWM_MAX) && (data.blue < PWM_MAX))
	{
		spin_lock(&rgb_lock);
		memcpy (&rgb, &data, sizeof (rgbdata_t));
		spin_unlock(&rgb_lock);
		printk (KERN_DEBUG "%s: %s - set r=%d g=%d b=%d pat=%d\n", MODNAME, __FUNCTION__, data.red, data.green, data.blue, data.pattern);
		return count;
	} 
	else 
	{
		printk (KERN_ERR "%s: %s bad data one of (r=%d,g=%d,b=%d). Should be 0-%d \n", MODNAME, __FUNCTION__, data.red, data.green, data.blue, PWM_MAX);
		return -1;
	}
}

int proc_set_hooter (struct file *filp,const char *buf,size_t count,loff_t *offp)
{
	hooter_t data;
	if (count != sizeof(hooter_t))
		printk (KERN_ERR "%s: %s - what the?! %d %d \n", MODNAME, __FUNCTION__, count, sizeof(hooter_t));
	if (copy_from_user (&data, buf, count) != 0) module_stats.copy_from_user_errors++;
	if (data == HOOTER_ON)
		schedule_set_pwm (HOOTER_INDEX, PWM_MAX-1);
	else
		schedule_set_pwm (HOOTER_INDEX, 0);
	return count;
}

int proc_set_head_ac220 (struct file *filp,const char *buf,size_t count,loff_t *offp)
{
	head_ac220_t data;
	if (count != sizeof(head_ac220_t))
		printk (KERN_ERR "%s: %s - what the?! %d %d \n", MODNAME, __FUNCTION__, count, sizeof(head_ac220_t));
	if (copy_from_user (&data, buf, count) != 0) module_stats.copy_from_user_errors++;
	if (data == HEAD_AC220_ON)
		schedule_set_pwm (HEAD_AC220_INDEX, PWM_MAX-1);
	else
		schedule_set_pwm (HEAD_AC220_INDEX, 0);
	return count;
}

struct file_operations proc_heater_fops = {
write: proc_set_heater,
};

struct file_operations proc_rgb_fops = {
write: proc_set_rgb,
};

struct file_operations proc_hooter_fops = {
write: proc_set_hooter,
};

struct file_operations proc_head_ac220_fops = {
write: proc_set_head_ac220,
};


// Rpi GPIO
int proc_get_estop (struct file *filp, char *buf, size_t count,loff_t *offp)
{
	estop_t estop;
	/*    if((int)*offp > 0)
	      {
	      return 0;
	      }*/
	estop = gpio_get_value(ESTOP_GPIO);
	if ( copy_to_user(buf, &estop, sizeof(estop)) != 0)
		printk ("%s copy_to_user failed!\n", MODNAME);	

	*offp += sizeof(estop);
	return sizeof(estop);
}

int read_estop(void)
{
	iostatus.estop = (estop_t) gpio_get_value(ESTOP_GPIO);
	//printk(KERN_INFO "%s: %s. estop=%d", MODNAME, __FUNCTION__, iostatus.estop);
	return 0;
}

int read_powercut(void)
{
	iostatus.powercut = (powercut_t) gpio_get_value(POWERCUT_GPIO);
	return 0;
}

int proc_get_powercut (struct file *filp, char *buf,size_t count,loff_t *offp)
{
	powercut_t powercut;
	/*if((int)*offp > 0)
	  {
	  return 0;
	  }*/
	powercut = gpio_get_value(POWERCUT_GPIO);
	if (copy_to_user(buf, &powercut, sizeof(powercut_t)) != 0)
		printk ("%s copy_to_user failed!\n", MODNAME);	

	*offp += sizeof(powercut_t);
	return sizeof(powercut_t);
}

struct file_operations proc_estop_fops = {
read: proc_get_estop,
};

struct file_operations proc_powercut_fops = {
read: proc_get_powercut,
};

// GPIO expander IOs
/* 
   write_proc_entry takes lock, sets val and sets changed=true, and releases lock.
   update_reg checks if changed = true, it writes the val to gpio expander port.
   then it takes lock, verifies that the val is still the same that was written to port,
   clears changed flag, and releases lock.
   */
int proc_get_microstep (struct file *filp, char *buf, size_t count, loff_t *offp)
{
	printk ("%s: in %s\n", MODNAME, __FUNCTION__);
	return count;
}

int proc_set_microstep (struct file *filp, const char *buf, size_t count, loff_t *offp)
{
	microstep_t data;
	printk (KERN_INFO "%s: in %s\n", MODNAME, __FUNCTION__);
	if (count != sizeof(microstep_t))
		printk (KERN_ERR "%s: %s - what the?! %d %d \n", MODNAME, __FUNCTION__, count, sizeof(microstep_t));

	if (copy_from_user (&data, buf, count) != 0) module_stats.copy_from_user_errors++;
	switch (data.axis)
	{
		case 0: 
			spin_lock(&gpiox2a_reg.lock);
			gpiox2a_reg.u.p2a.microstep_x = 0x07 & data.value; 
			gpiox2a_reg.changed = true; 
			spin_unlock(&gpiox2a_reg.lock);
			break;
		case 1:  
			spin_lock(&gpiox2b_reg.lock);
			gpiox2b_reg.u.p2b.microstep_y = 0x07 & data.value; 
			gpiox2b_reg.changed = true; 
			spin_unlock(&gpiox2b_reg.lock);
			break;
		case 2:  
			spin_lock(&gpiox2b_reg.lock);
			gpiox2b_reg.u.p2b.microstep_z = 0x07 & data.value; 
			gpiox2b_reg.changed = true; 
			spin_unlock(&gpiox2b_reg.lock);
			break;
		default: break;
	}
	return count;
}

int proc_set_door (struct file *filp,const char *buf,size_t count,loff_t *offp)
{
	doordata_t data;
	printk (KERN_INFO "%s: in %s\n", MODNAME, __FUNCTION__);
	if (count != sizeof(doordata_t))
		printk (KERN_ERR "%s: %s - what the?! %d %d \n", MODNAME, __FUNCTION__, count, sizeof(door_t));
	if (copy_from_user (&data, buf, count) != 0) module_stats.copy_from_user_errors++;
	switch (data.door)
	{
		case FRONT_DOOR:
			spin_lock(&gpiox2a_reg.lock);
			gpiox2a_reg.u.p2a.close_door = (int)data.value; 
			gpiox2a_reg.changed = true;
			spin_unlock(&gpiox2a_reg.lock);
			break;
		case FILAMENT_DOOR:	
		case TOP_DOOR:	
		case BAKER_DOOR:	
			printk ("%s: %s not implemented yet!\n", MODNAME, __FUNCTION__);
			break;
	}
	return count;
}

void set_axis(axisdata_t data)
{
	spin_lock(&gpiox2a_reg.lock);
	switch (data.axis)
	{
		case 0: 
			gpiox2a_reg.u.p2a.x_enable = (int)data.enable; gpiox2a_reg.changed = true; break;
		case 1: 
			gpiox2a_reg.u.p2a.y_enable = (int)data.enable; gpiox2a_reg.changed = true; break;
		case 2:  
			gpiox2a_reg.u.p2a.z_enable = (int)data.enable; gpiox2a_reg.changed = true; break;
		default: break;
	}
	spin_unlock(&gpiox2a_reg.lock);
}

int proc_set_axis (struct file *filp,const char *buf,size_t count,loff_t *offp)
{
	axisdata_t data;
	if (count != sizeof(axisdata_t))
		printk (KERN_ERR "%s: %s - what the?! %d %d \n", MODNAME, __FUNCTION__, count, sizeof(axisdata_t));
	if (copy_from_user (&data, buf, count) != 0) module_stats.copy_from_user_errors++;
	set_axis(data);
	return count;
}

void turn_off_all_axes(void)
{
	axisdata_t data;

	data.axis = X_AXIS;
	data.enable = false;
	set_axis(data);

	data.axis = Y_AXIS;
	data.enable = false;
	set_axis(data);

	data.axis = Z_AXIS;
	data.enable = false;
	set_axis(data);
};

struct file_operations proc_axis_fops = {
write: proc_set_axis,
};

struct file_operations proc_door_fops = {
write: proc_set_door,
};

struct file_operations proc_microstep_fops = {
read: proc_get_microstep,
      write: proc_set_microstep,
};

// software
int proc_get_iostatus (struct file *filp, char *buf, size_t count, loff_t *offp)
{
	/*    if((int)*offp > 0)
	      {
	      return 0;
	      }*/
	//printk(KERN_INFO "%s: in %s", MODNAME, __FUNCTION__);
	if (copy_to_user(buf, &iostatus, sizeof(iostatus_t)) != 0)
	{
		printk ("%s copy_to_user failed!\n", MODNAME);
		return -1;
	}
	*offp += sizeof(iostatus_t);
	return sizeof(iostatus_t);
}

int proc_set_resethw (struct file *filp, const char *buf, size_t count, loff_t *offp)
{	
	reset_t data;

	printk (KERN_INFO "%s: in %s\n", MODNAME, __FUNCTION__);
	if (count != sizeof(reset_t))
		printk (KERN_ERR "%s: %s - what the?! %d %d \n", MODNAME, __FUNCTION__, count, sizeof(reset_t));

	if (copy_from_user (&data, buf, count) != 0) module_stats.copy_from_user_errors++;
	printk (KERN_INFO "%s: %s setting reset_status = %d\n", MODNAME, __FUNCTION__, (int)data);
	iostatus.reset_status = data;

	return count;
}

int proc_set_resetrtd (struct file *filp, const char *buf, size_t count, loff_t *offp)
{	
	reset_rtd_t data;

	printk (KERN_INFO "%s: in %s\n", MODNAME, __FUNCTION__);
	if (count != sizeof(reset_rtd_t))
		printk (KERN_ERR "%s: %s - what the?! %d %d \n", MODNAME, __FUNCTION__, count, sizeof(reset_rtd_t));

	if (copy_from_user (&data, buf, count) != 0) module_stats.copy_from_user_errors++;
	printk (KERN_INFO "%s: %s setting reset_status = %d\n", MODNAME, __FUNCTION__, (int)data);
	iostatus.reset_i2c0 = data;

	return count;
}
int proc_set_testmode (struct file *filp, const char *buf, size_t count, loff_t *offp)
{	
	testmode_t data;

	printk (KERN_INFO "%s: in %s\n", MODNAME, __FUNCTION__);
	if (count != sizeof(testmode_t))
		printk (KERN_ERR "%s: %s - what the?! %d %d \n", MODNAME, __FUNCTION__, count, sizeof(testmode_t));

	if (copy_from_user (&data, buf, count) != 0) module_stats.copy_from_user_errors++;
	printk (KERN_INFO "%s: %s setting reset_status = %d\n", MODNAME, __FUNCTION__, (int)data);
	test=data;
	iostatus.testmode = data;

	return count;
}

int proc_set_enablehw (struct file *filp, const char *buf, size_t count, loff_t *offp)
{
	enable_t data;

	printk (KERN_INFO "%s: in %s\n", MODNAME, __FUNCTION__);
	if (count != sizeof(enable_t))
		printk (KERN_ERR "%s: %s - what the?! %d %d \n", MODNAME, __FUNCTION__, count, sizeof(enable_t));

	if (copy_from_user (&data, buf, count) != 0) module_stats.copy_from_user_errors++;
	//printk (KERN_INFO "%s: %s setting enabled_status = %d\n", MODNAME, __FUNCTION__, (int)data);
	iostatus.enabled_status = data;
	return count;
}

int proc_get_stats (struct file *filp, char *buf, size_t count,loff_t *offp)
{
	/*    if((int)*offp > 0)
	      {
	      return 0;
	      }*/

	if (copy_to_user(buf, &module_stats, sizeof(stats_t)) != 0)
		printk ("%s copy_to_user failed!\n", MODNAME);	

	*offp += sizeof(stats_t);
	return sizeof(stats_t);
}

int proc_set_watchdog_timeout (struct file *filp, const char *buf, size_t count, loff_t *offp)
{
	int watchdog_timeout;

	if (count != sizeof(int))
	{
		printk (KERN_ERR "%s: %s - what the?! %d %d \n", MODNAME, __FUNCTION__, count, sizeof(enable_t));
		return count;
	}
	//printk (KERN_INFO "%s: %s got timeout = %d\n", MODNAME, __FUNCTION__, watchdog_timeout);

	if (copy_from_user (&watchdog_timeout, buf, count) != 0) module_stats.copy_from_user_errors++;
	iostatus.watchdog_timer = watchdog_timeout;
	return count;
}

struct file_operations proc_iostatus_fops = {
read: proc_get_iostatus,
};

struct file_operations proc_resethw_fops = {
write: proc_set_resethw,
};

struct file_operations proc_resetrtd_fops = {
write: proc_set_resetrtd,
};

struct file_operations proc_testmode_fops = {
write: proc_set_testmode,
};

struct file_operations proc_enablehw_fops = {
write: proc_set_enablehw,
};

struct file_operations proc_get_stats_fops = {
read: proc_get_stats,
};

struct file_operations proc_set_wdt_fops = {
write: proc_set_watchdog_timeout,
};


void remove_proc_entries(void) 
{
	printk ("%s: in %s\n", MODNAME, __FUNCTION__);
	//parent = NULL; {
	if (parent) {
	remove_proc_entry (PROC_RTD_FILENAME, parent);
	remove_proc_entry (PROC_COUNTERS_FILENAME, parent);
	remove_proc_entry (PROC_HEATER_FILENAME, parent);
	remove_proc_entry (PROC_RGB_FILENAME, parent);
	remove_proc_entry (PROC_HOOTER_FILENAME, parent);
	remove_proc_entry (PROC_HEAD_AC220_FILENAME, parent);
	remove_proc_entry (PROC_ESTOP_FILENAME, parent);
	remove_proc_entry (PROC_POWERCUT_FILENAME, parent);
	remove_proc_entry (PROC_AXIS_FILENAME, parent);
	remove_proc_entry (PROC_DOOR_FILENAME, parent);
	remove_proc_entry (PROC_MICROSTEP_FILENAME, parent);
//	remove_proc_entry (PROC_RFID0_FILENAME, parent);
//	remove_proc_entry (PROC_RFID1_FILENAME, parent);
	remove_proc_entry (PROC_IOSTATUS_FILENAME, parent);
	remove_proc_entry (PROC_RESETHW_FILENAME, parent);
	remove_proc_entry (PROC_RESETRTD_FILENAME, parent);
	remove_proc_entry (PROC_TESTMODE_FILENAME, parent);
	remove_proc_entry (PROC_ENABLEHW_FILENAME, parent);
	remove_proc_entry (PROC_STATISTICS_FILENAME, parent);
	remove_proc_entry (PROC_WATCHDOG_TIMEOUT_FILENAME, parent);
	remove_proc_entry (PROC_DIRNAME, NULL);
	};
};

int create_proc_entries(void) 
{
	printk (KERN_INFO "%s: in %s\n", MODNAME, __FUNCTION__);
	parent = proc_mkdir(PROC_DIRNAME, NULL);
	if (parent == NULL) {printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__, PROC_DIRNAME); return -1;}
	//parent = NULL;
	if (proc_create (PROC_RTD_FILENAME, S_IRWXUGO,     parent, &proc_rtd_fops) == NULL)         
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_RTD_FILENAME); return -1;}
	if (proc_create (PROC_COUNTERS_FILENAME, S_IRWXUGO,   parent, &proc_counters_fops) == NULL) 
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_COUNTERS_FILENAME); return -1;}
	if (proc_create (PROC_HEATER_FILENAME, S_IRWXUGO,     parent, &proc_heater_fops) == NULL)   
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_HEATER_FILENAME); return -1;}
	if (proc_create (PROC_RGB_FILENAME, S_IRWXUGO,        parent, &proc_rgb_fops) == NULL)      
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_RGB_FILENAME); return -1;}
	if (proc_create (PROC_HOOTER_FILENAME, S_IRWXUGO,     parent, &proc_hooter_fops) == NULL)   
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_HOOTER_FILENAME); return -1;}
	if (proc_create (PROC_HEAD_AC220_FILENAME, S_IRWXUGO, parent, &proc_head_ac220_fops) == NULL)
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_HEAD_AC220_FILENAME); return -1;}
	if (proc_create (PROC_ESTOP_FILENAME, S_IRWXUGO,      parent, &proc_estop_fops) == NULL)    
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_ESTOP_FILENAME); return -1;}
	if (proc_create (PROC_POWERCUT_FILENAME, S_IRWXUGO,   parent, &proc_powercut_fops) == NULL) 
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_POWERCUT_FILENAME); return -1;}
	if (proc_create (PROC_AXIS_FILENAME, S_IRWXUGO,       parent, &proc_axis_fops) == NULL)     
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_AXIS_FILENAME); return -1;}
	if (proc_create (PROC_DOOR_FILENAME, S_IRWXUGO,       parent, &proc_door_fops) == NULL)     
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_DOOR_FILENAME); return -1;}
	if (proc_create (PROC_MICROSTEP_FILENAME, S_IRWXUGO,  parent, &proc_microstep_fops) == NULL)
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_MICROSTEP_FILENAME); return -1;}
	if (proc_create (PROC_IOSTATUS_FILENAME, S_IRWXUGO,   parent, &proc_iostatus_fops) == NULL) 
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_IOSTATUS_FILENAME); return -1;}
	if (proc_create (PROC_RESETHW_FILENAME, S_IRWXUGO,    parent, &proc_resethw_fops) == NULL)  
		{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_RESETHW_FILENAME); return -1;}
	if (proc_create (PROC_RESETRTD_FILENAME, S_IRWXUGO,    parent, &proc_resetrtd_fops) == NULL)  
		{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_RESETRTD_FILENAME); return -1;}
	if (proc_create (PROC_TESTMODE_FILENAME, S_IRWXUGO,    parent, &proc_testmode_fops) == NULL)  
		{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_TESTMODE_FILENAME); return -1;}
	if (proc_create (PROC_ENABLEHW_FILENAME, S_IRWXUGO,   parent, &proc_enablehw_fops) == NULL) 
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_ENABLEHW_FILENAME); return -1;}
	if (proc_create (PROC_STATISTICS_FILENAME, S_IRWXUGO, parent, &proc_get_stats_fops) == NULL)
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_STATISTICS_FILENAME); return -1;}
	if (proc_create (PROC_WATCHDOG_TIMEOUT_FILENAME, S_IRWXUGO, parent, &proc_set_wdt_fops) == NULL)
	{printk (KERN_ERR "%s: %s %s\n", MODNAME, __FUNCTION__,  PROC_WATCHDOG_TIMEOUT_FILENAME); return -1;}
	return 0;
};

int set_cs_rtd(int num)
{
	/* send num to set CS of corresponding RTD chip, starting from zero. Any invalid number to clear all CS.*/
	/* we dont have to worry about disturbing values on input pins because the register itself is different on gpioX*/
	int ret = 0;
	buf0[0]=0x14;
	switch (num)
	{
		case 0: buf0[1] = 0xf0; break;
		case 1: buf0[1] = 0xe8; break;
		case 2: buf0[1] = 0xd8; break;
		case 3: buf0[1] = 0xb8; break;
		case 4: buf0[1] = 0x78; break;
		default: buf0[1] = 0xf8; break;
	}
	//	printk (KERN_INFO "%s: %s will set 0x%02x 0x%02x\n", MODNAME , __FUNCTION__, buf0[0], buf0[1]);

	i2cmsg[0].addr = GPIOX1;
	i2cmsg[0].flags = 0;
	i2cmsg[0].len = 2;
	i2cmsg[0].buf = buf0;
	if (pAdap)  {
		if (bcm2835_i2c_xfer(pAdap, i2cmsg, 1) < 0) 
		{
			module_stats.i2c_errors++; 
			printk (KERN_ERR "%s: %s failed", MODNAME, __FUNCTION__);
			return -1;
		}
       		else 
		{
			module_stats.i2c_transfers++; 
		}
	}
		else return -1;
	return 0;
}

int set_gpiox_reg(int dev_address, port_t port, uint8_t val)
{
	switch (port)
	{
		case PORT_A: buf0[0] = 0x14; break;
		case PORT_B: buf0[0] = 0x15; break;
		default: return -1; break;
	}
	buf0[1] = val;
//	printk (KERN_INFO "%s: %s will set. dev_addr: %02x data %02x %02x\n", MODNAME , __FUNCTION__, dev_address, buf0[0], buf0[1]);

	i2cmsg[0].addr = dev_address;
	i2cmsg[0].flags = 0;
	i2cmsg[0].len = 2;
	i2cmsg[0].buf = buf0;
	
	if (pAdap)  
	{
		if (bcm2835_i2c_xfer(pAdap, i2cmsg, 1) < 0) 
		{
			module_stats.i2c_errors++; 
			printk (KERN_ERR "%s: %s failed", MODNAME, __FUNCTION__);
			return -1;
		}
       		else 
		{
			module_stats.i2c_transfers++;
		}
	}
	else return -1;
	return 0;
}

int read_spi1(void)
{
	struct spi_transfer tfr;
	struct spi_message msg;
	msg.spi=pSpi1;
	bcm2835aux_spi_prepare_message(pMaster1, &msg);

	tfr.speed_hz = 10000000;
	tfr.tx_buf = &tx_ctrbuf;
	tfr.rx_buf = &rx_ctrbuf;
	tfr.len = sizeof(rx_ctrbuf);
	if (bcm2835aux_spi_transfer_one(pMaster1, pSpi1, &tfr) == 0)
	{
		module_stats.spi1_transfers++;
		/*		printk (KERN_INFO "%s: %02x %08x %08x %08x %08x %08x\n", MODNAME,
				rx_ctrbuf.cksum, 
				rx_ctrbuf.counter[0].data32, 
				rx_ctrbuf.counter[1].data32, 
				rx_ctrbuf.counter[2].data32, 
				rx_ctrbuf.counter[3].data32,
				rx_ctrbuf.counter[4].data32);*/
		return 0;
	}
	else
	{
		module_stats.spi1_errors++;
		return -1;
	}
}

int cfg_rtd_chips(void)
{
	struct spi_transfer tfr;
	struct spi_message msg;
	int i, ret;
	msg.spi=pSpi0;

	/* configure RTD0 chip over SPI*/
	init_buf[0] = 0x80; init_buf[1] = 0xc2;
	tfr.speed_hz = 5000;
	tfr.tx_buf = init_buf;
	tfr.rx_buf = rxbuf;
	tfr.len = 2;

	for (i=0; i<NUM_RTDS; i++)
	{	
		udelay (1000);
		bcm2835_spi_prepare_message(pMaster0, &msg);
		if (set_cs_rtd(i) == 0)
		{	ret = bcm2835_spi_transfer_one(pMaster0, pSpi0, &tfr);
			if (ret < 0) 
			{
				printk (KERN_ERR "%s: %s spi xfer error", MODNAME, __FUNCTION__);
				module_stats.spi0_errors++;
			}
			else 
			{
				if (set_cs_rtd(-1) != 0) 
				{
					printk (KERN_ERR "%s: %s cs clr error", MODNAME, __FUNCTION__);
				}
				else module_stats.spi0_transfers++;
			}
		}
	}
	return 0;
}

/*
 * The CS of rtd mysteriously goes low during runtime, and thats why temperature readings go bad.
 * When taht happens, this fn will be called to reinitialize the i2c chip.
 * stats will be maintained, so we can observe we're not doing this too many times.
 *
 */
int init_i2c0(void)
{
	int ret = 0;
	printk(KERN_INFO "%s: in %s", MODNAME, __FUNCTION__);
	i2cmsg[0].flags = 0;
	i2cmsg[0].len = 2;
	i2cmsg[0].buf = buf0;
	i2cmsg[0].addr = GPIOX1;

	buf0[0]=0x00; buf0[1]=0x07;
	if (pAdap) {
		ret = bcm2835_i2c_xfer(pAdap, i2cmsg, 1);
		if (ret < 0) {
			printk (KERN_ERR "%s: %s line %d, ret=%d", MODNAME, __FUNCTION__, __LINE__, ret);
			module_stats.i2c_errors++;
		}
		else {
			module_stats.i2c_transfers++;}
	}
	else 
	{
		printk(KERN_ERR "%s: %s line %d pAdap missing!\n", MODNAME, __FUNCTION__, __LINE__);
		ret = -1;
	}
	if (ret < 0) {
		return ret;
	}

	buf0[0]=0x01; buf0[1]=0x3f;
	if (pAdap)  {
		ret = bcm2835_i2c_xfer(pAdap, i2cmsg, 1);
		if (ret < 0) {
			printk (KERN_ERR "%s: %s line %d, ret=%d", MODNAME, __FUNCTION__, __LINE__, ret);
			module_stats.i2c_errors++;
		}
		else {
			module_stats.i2c_transfers++;
		}
	}
	else
	{
		printk(KERN_ERR "%s: %s line %d pAdap missing!\n", MODNAME, __FUNCTION__, __LINE__);
		ret = -1;
	}
	module_stats.i2c0_reset_count++;
	return ret;
}

int init_i2c_peripherals(void)
{
	int ret;
	printk(KERN_INFO "%s: in %s", MODNAME, __FUNCTION__);
	i2cmsg[0].flags = 0;
	i2cmsg[0].len = 2;
	i2cmsg[0].buf = buf0;
	i2cmsg[0].addr = GPIOX1;

	buf0[0]=0x00; buf0[1]=0x07;
	if (pAdap) {
		ret = bcm2835_i2c_xfer(pAdap, i2cmsg, 1);
		if (ret < 0) {
			printk (KERN_ERR "%s: %s line %d, ret=%d", MODNAME, __FUNCTION__, __LINE__, ret);
			module_stats.i2c_errors++; return -1;
		}
		else module_stats.i2c_transfers++;
	}
	else 
	{
		printk(KERN_INFO "%s: %s line %d pAdap missing!\n", MODNAME, __FUNCTION__, __LINE__);
		return -1;
	}

	buf0[0]=0x01; buf0[1]=0x3f;
	if (pAdap)  {
		ret = bcm2835_i2c_xfer(pAdap, i2cmsg, 1);
		if (ret < 0) {
			printk (KERN_ERR "%s: %s line %d, ret=%d", MODNAME, __FUNCTION__, __LINE__, ret);
			module_stats.i2c_errors++; return -1;
		}
		else module_stats.i2c_transfers++;
	}
	else
	{
		printk(KERN_INFO "%s: %s line %d pAdap missing!\n", MODNAME, __FUNCTION__, __LINE__);
		return -1;
	}

	i2cmsg[0].addr = GPIOX2;

	buf0[0]=0x00; buf0[1]=0x02;
	if (pAdap)  {
		ret = bcm2835_i2c_xfer(pAdap, i2cmsg, 1);
		if (ret < 0) {
			printk (KERN_ERR "%s: %s line %d, ret=%d", MODNAME, __FUNCTION__, __LINE__, ret);
			module_stats.i2c_errors++; return -1;
		}
		else module_stats.i2c_transfers++;
	}
	else
	{
		printk(KERN_INFO "%s: %s line %d pAdap missing!\n", MODNAME, __FUNCTION__, __LINE__);
		return -1;
	}

	buf0[0]=0x01; buf0[1]=0x00;
	if (pAdap)  {
		ret = bcm2835_i2c_xfer(pAdap, i2cmsg, 1);
		if (ret < 0) {
			printk (KERN_ERR "%s: %s line %d, ret=%d", MODNAME, __FUNCTION__, __LINE__, ret);
			module_stats.i2c_errors++; return -1;
		}
		else module_stats.i2c_transfers++;
	}
	else
	{
		printk(KERN_INFO "%s: %s line %d pAdap missing!\n", MODNAME, __FUNCTION__, __LINE__);
		return -1;
	}

	return 0;
}

int read_rtd(int n)
{
	/*
	   WARNING - Shit ahead.
	   This function transfer_one doesnt complete the xfer before it returns. So rx/tx buffers are not yet ready to use.
	   however in this specific application we just want to read the RTD values, never write back. and we are not
	   very worried about freshness of values. and on top, there is a global two dimensional array which cycles 
	   pointers  at a low frequency, so we know that consecutive transfers use different memory locations which are 
	   always valid. so we are choosing to live with this shit. 
	   */
	struct spi_transfer tfr;
	struct spi_message msg;
	int ret;

	//printk(KERN_INFO "%s: in %s, n=%d", MODNAME, __FUNCTION__, n);
	if (n<0 || n > 4)
	{
		printk(KERN_INFO "%s: %s, n=%d", MODNAME, __FUNCTION__, n);
		return -1;
	}
	msg.spi=pSpi0;

	tfr.speed_hz = RTD_SPISPEED;
	tfr.tx_buf = txbuf;
	tfr.rx_buf = rx_rtdbuf.buf[n];
	tfr.len = RTD_XFERLEN;

	bcm2835_spi_prepare_message(pMaster0, &msg);
	if(set_cs_rtd(n)==0)
	{
		ret = bcm2835_spi_transfer_one(pMaster0, pSpi0, &tfr);
		if (ret < 0) 
		{
			printk (KERN_ERR "%s: %s spi xfer error", MODNAME, __FUNCTION__);
			module_stats.spi0_errors++;
		}
		else 
		{
			if (set_cs_rtd(-1) != 0) 
			{
				printk (KERN_ERR "%s: %s cs clear error", MODNAME, __FUNCTION__);
			}
			else module_stats.spi0_transfers++;
		}
	}
	return 0;
}

int read_door_latch(void)
{
	//	printk(KERN_INFO "%s: in %s", MODNAME, __FUNCTION__);
	buf0[0]=0x12;

	i2cmsg[0].addr = GPIOX2;
	i2cmsg[0].flags = 0;
	i2cmsg[0].len = 1;
	i2cmsg[0].buf = buf0;

	i2cmsg[1].addr = GPIOX2;
	i2cmsg[1].flags = I2C_M_RD;
	i2cmsg[1].len = 1;
	i2cmsg[1].buf = buf1;

	if (pAdap){
		if (bcm2835_i2c_xfer(pAdap, i2cmsg, 2) < 0) {
			module_stats.i2c_errors++;
			return -1;
		}
		else module_stats.i2c_transfers++;
	}
	else return -1;

	iostatus.front_door_status = ((0x02 & buf1[0]) >> 1);
	return 0;
}

int read_fault_pins(void)
{
	bitfields_t tmp;
	//printk(KERN_INFO "%s: in %s", MODNAME, __FUNCTION__);
	buf0[0]=0x12;

	i2cmsg[0].addr = GPIOX1;
	i2cmsg[0].flags = 0;
	i2cmsg[0].len = 1;
	i2cmsg[0].buf = buf0;

	i2cmsg[1].addr = GPIOX1;
	i2cmsg[1].flags = I2C_M_RD;
	i2cmsg[1].len = 1;
	i2cmsg[1].buf = buf1;


	if (pAdap)
	{
		if (bcm2835_i2c_xfer(pAdap, i2cmsg, 2) < 0) {
			module_stats.i2c_errors++;
			printk (KERN_ERR "%s: %s failed", MODNAME, __FUNCTION__);
			return -1;
		}
		else module_stats.i2c_transfers++;
	}
	else return -1;

	tmp.val = buf1[0];

	//printk(KERN_INFO "%s: %s val = %02x", MODNAME, __FUNCTION__, tmp.val);

	iostatus.xfault = tmp.p1a.x_fault;
	iostatus.yfault = tmp.p1a.y_fault;
	iostatus.zfault = tmp.p1a.z_fault;

	return 0;
}


int read_lim_swiches(void)
{	
	bitfields_t tmp;
	//printk(KERN_INFO "%s: in %s", MODNAME, __FUNCTION__);
	buf0[0]=0x13;

	i2cmsg[0].addr = GPIOX1;
	i2cmsg[0].flags = 0;
	i2cmsg[0].len = 1;
	i2cmsg[0].buf = buf0;

	i2cmsg[1].addr = GPIOX1;
	i2cmsg[1].flags = I2C_M_RD;
	i2cmsg[1].len = 1;
	i2cmsg[1].buf = buf1;


	if (pAdap) {
		if (bcm2835_i2c_xfer(pAdap, i2cmsg, 2) < 0) {
			module_stats.i2c_errors++;
			printk (KERN_ERR "%s: %s failed", MODNAME, __FUNCTION__);
			return -1;
		}
		else module_stats.i2c_transfers++;
	}
	else
	{
		module_stats.i2c_errors++;
		return -1;
	}

	tmp.val = buf1[0];
	//printk(KERN_INFO "%s: %s val = %02x", MODNAME, __FUNCTION__, tmp.val);

	iostatus.xmin_hit = tmp.p1b.x_min;
	iostatus.xmax_hit = tmp.p1b.x_max;
	iostatus.ymin_hit = tmp.p1b.y_min;
	iostatus.ymax_hit = tmp.p1b.y_max;
	iostatus.zmin_hit = tmp.p1b.z_min;
	iostatus.zmax_hit = tmp.p1b.z_max;

	return 0;
}

/*
   int reset_rfid(char num, bool enable)
   {
   int ret = 0;
   spin_lock(&gpiox2b_reg.lock);
   switch (axis)
   {
   case 0: 
   gpiox2b_reg.u.p2b.reset_rfid0 = (int)enable; gpiox2b_reg.changed = true; break;
   case 1:  
   gpiox2b_reg.u.p2b.reset_rfid1 = (int)enable; gpiox2b_reg.changed = true; break;
   default: ret=-1; break;
   }
   spin_unlock(&gpiox2b_reg.lock);
   return ret;
   }

   static unsigned long cur_us(void)
   {
   struct timeval tv;
   do_gettimeofday(&tv);
   return 1000000 * tv.tv_sec + tv.tv_usec;
   }
   */

void set_rgb(colour_t colour, pattern_t pattern)
{
	spin_lock(&rgb_lock);
	rgb.pattern = pattern;
	switch (colour)
	{
		case RED:
			rgb.red   = PWM_MAX - 1;
			rgb.green = 0;
			rgb.blue  = 0;
			break;
		case GREEN:
			rgb.green = PWM_MAX - 1;
			rgb.red = 0;
			rgb.blue  = 0;
			break;
		case BLUE:
			rgb.blue   = PWM_MAX - 1;
			rgb.green = 0;
			rgb.red  = 0;
			break;
		case MAGENTA:
			rgb.red   = PWM_MAX - 1;
			rgb.blue   = PWM_MAX - 1;
			rgb.green = 0;
			break;
		case CYAN:
			rgb.red  = 0;
			rgb.blue   = PWM_MAX - 1;
			rgb.green = PWM_MAX - 1;
			break;

		case WHITE:
			rgb.blue   = PWM_MAX - 1;
			rgb.green = PWM_MAX - 1;
			rgb.red   = PWM_MAX - 1;
			break;
		case OFF:
		default:
			rgb.red   = 0;
			rgb.green = 0;
			rgb.blue  = 0;
	}
	spin_unlock(&rgb_lock);
}

void handle_hw_error(void)
{
	iostatus.is_enabled = false;
	turn_off_all_pwm_channels();
	turn_off_all_axes();
	mdelay(5);
	RGB_HW_ERROR;
}

void turn_off_everything(void)
{
	turn_off_all_pwm_channels();
	turn_off_all_axes();
}

int initialize_hardware(void)
{
	int ret;

	gpio_direction_input(ESTOP_GPIO);
	gpio_direction_input(POWERCUT_GPIO);

	if (bcm2835_spi_get_master(&pMaster0) != 0) {
		printk(KERN_ERR "%s:%s: cant get SPI0 master\n", MODNAME, __FUNCTION__); return -1;
	}	

	if (bcm2835_spi_get_spi(&pSpi0) != 0) {
		printk(KERN_WARNING "%s:%s: cant get SPI0 spi but its ok.\n", MODNAME, __FUNCTION__);
	}

	if (bcm2835aux_spi_get_master(&pMaster1) != 0) {
		printk(KERN_ERR "%s:%s: cant get SPI1 master\n", MODNAME, __FUNCTION__); return -1;
	}	

	if (bcm2835aux_spi_get_spi(&pSpi1) != 0) {
		printk(KERN_WARNING "%s:%s: cant get SPI1 spi but its ok.\n", MODNAME, __FUNCTION__);
	}	

	if (bcm2835_i2c_get_adapter(&pAdap) != 0) {
		printk(KERN_ERR "%s:%s: cant get I2C adapter\n", MODNAME, __FUNCTION__); return -1;
	}	

	if (init_i2c_peripherals() != 0) {
		printk(KERN_ERR "%s:%s: cant init i2c peripherals\n", MODNAME, __FUNCTION__); return -1;
	}

	ret = cfg_rtd_chips();
	printk (KERN_INFO "%s: cfg_rtd_chips ret=%d \n", MODNAME, ret);
	ret = init_pwm_chip();
	printk (KERN_INFO "%s: init_pwm_chip ret=%d \n", MODNAME, ret);
	ret = set_pwm_freq();
	printk (KERN_INFO "%s: set_pwm_freq ret=%d \n", MODNAME, ret);
	return ret;
}

int initialize_values(void)
{
	iostatus.watchdog_timer = -1;
	rgb.pattern = LIGHT_OFF;
	return 0;
}

void clear_all_stats(void)
{
	memset (&module_stats, 0, sizeof(stats_t));
}

void clear_all_errors(void)
{
	iostatus.i2c_hardware_error = false;
	iostatus.spi0_hardware_error = false;
	iostatus.spi1_hardware_error = false;
	iostatus.watchdog_timeout_error = false;
	iostatus.hardware_error = false;
}

static enum hrtimer_restart fast_timer_callback (struct hrtimer *timer) 
{
	ktime_t ktime, now;
	uint64_t overrun;
	static reset_t old_reset;
	int ret;
	ktime = ktime_set(0,small_delay);

	//printk(KERN_INFO "%s: in %s.", MODNAME, __FUNCTION__);

	now = hrtimer_cb_get_time(timer);
	overrun = hrtimer_forward(timer, now, ktime);

	if (!spin_trylock(&fast_timer_callback_lock)) 
	{
		printk(KERN_INFO "%s: %s didnt get lock. restarting timer.\n", MODNAME, __FUNCTION__);
		return HRTIMER_RESTART;
	}

	if (iostatus.reset_status)
	{
		if (old_reset == false) // do resetting things on rising edge of reset signal
		{
			initialize_hardware();
			turn_off_everything();
			clear_all_stats();
			clear_all_errors();
			iostatus.is_enabled = false;
			RGB_HW_DISABLED;
			old_reset = true;
		}
		goto fast_timer_callback_unlock_and_restart;
	}
	old_reset = false;

	if (iostatus.reset_i2c0)
	{
		iostatus.reset_i2c0=0;

		if (init_i2c0() < 0) {
			printk(KERN_ERR "%s:%s: cant init i2c0 peripherals\n", MODNAME, __FUNCTION__);
		}
		goto fast_timer_callback_unlock_and_restart;
	}

	if (iostatus.hardware_error) 
	{
		iostatus.is_enabled = false;
		RGB_HW_ERROR;
		goto fast_timer_callback_unlock_and_restart;
	}

	if (!iostatus.enabled_status) 
	{
		iostatus.is_enabled = false;
		RGB_HW_DISABLED;
	}
	else
	{
		iostatus.is_enabled = true;
		RGB_HW_ENABLED;
	}

	ret = read_spi1();
	if (ret) {
		printk (KERN_ERR "%s: read_spi1 failed: %d\n", MODNAME, ret);
	}

fast_timer_callback_unlock_and_restart:
	spin_unlock(&fast_timer_callback_lock);
	return HRTIMER_RESTART;
}

static enum hrtimer_restart slow_timer_callback (struct hrtimer *timer) 
{
	ktime_t ktime, now;
	uint64_t overrun;
	static uint32_t ctr = 0;
	ktime = ktime_set(0,large_delay);

	//printk(KERN_INFO "%s: in %s. en=%d\n", MODNAME, __FUNCTION__, iostatus.is_enabled);

	now = hrtimer_cb_get_time(timer);
	overrun = hrtimer_forward(timer, now, ktime);

	if (!spin_trylock(&slow_timer_callback_lock)) 
	{
		printk(KERN_INFO "%s: %s didnt get lock. restarting timer.\n", MODNAME, __FUNCTION__);
		return HRTIMER_RESTART;
	}

#define EVERY_NTH_TIME 20
	ctr++;
	if (!((ctr+0) % EVERY_NTH_TIME)) {read_rtd(0);}
	if (!((ctr+1) % EVERY_NTH_TIME)) {read_rtd(1);}
	if (!((ctr+2) % EVERY_NTH_TIME)) {read_rtd(2);}
	if (!((ctr+3) % EVERY_NTH_TIME)) {read_rtd(3);}
	if (!((ctr+4) % EVERY_NTH_TIME)) {read_rtd(4);}
	if (!((ctr+5) % EVERY_NTH_TIME)) {read_lim_swiches(); }
	if (!((ctr+6) % EVERY_NTH_TIME)) {read_fault_pins(); }
	if (!((ctr+7) % EVERY_NTH_TIME)) {read_estop(); read_powercut();}
	if (!((ctr+8) % EVERY_NTH_TIME)) { set_pwm(RGB_R_INDEX); }
	if (!((ctr+9) % EVERY_NTH_TIME)) { set_pwm(RGB_G_INDEX); }
	if (!((ctr+10) % EVERY_NTH_TIME)) { set_pwm(RGB_B_INDEX); }
	//write stuff only if enabled
	if (iostatus.is_enabled)
	{		
		if (!((ctr+11) % EVERY_NTH_TIME)) 
		{
			if (gpiox1b_reg.changed)
			{
				val8 = gpiox1b_reg.u.val;
				set_gpiox_reg(GPIOX1, PORT_B, val8);
				//now safely clear changed flag if data is consistent
				spin_lock(&gpiox1b_reg.lock);
				if (gpiox1b_reg.u.val == val8) gpiox1b_reg.changed = false;
				//else never mind. we will get next opportunity in next cycle
				spin_unlock(&gpiox1b_reg.lock);
			}
		}
		if (!((ctr+12) % EVERY_NTH_TIME)) 
		{
			if (gpiox2a_reg.changed)
			{
				val8 = gpiox2a_reg.u.val;
				set_gpiox_reg(GPIOX2, PORT_A, val8);
				spin_lock(&gpiox2a_reg.lock);
				if (gpiox2a_reg.u.val == val8) gpiox2a_reg.changed = false;
				spin_unlock(&gpiox2a_reg.lock);
			}
		}
		if (!((ctr+13) % EVERY_NTH_TIME)) 
		{
			if (gpiox2b_reg.changed)
			{
				val8 = gpiox2b_reg.u.val;
				set_gpiox_reg(GPIOX2, PORT_B, val8);
				spin_lock(&gpiox2b_reg.lock);
				if (gpiox2b_reg.u.val == val8) gpiox2b_reg.changed = false;
				spin_unlock(&gpiox2b_reg.lock);
			}
		}
		if (!((ctr+14) % EVERY_NTH_TIME)) { set_pwm(HEATER_INDEX+0); }
		if (!((ctr+15) % EVERY_NTH_TIME)) { set_pwm(HEATER_INDEX+1); }
		if (!((ctr+16) % EVERY_NTH_TIME)) { set_pwm(HEATER_INDEX+2); }
		if (!((ctr+17) % EVERY_NTH_TIME)) { set_pwm(HEATER_INDEX+3); }
		if (!((ctr+18) % EVERY_NTH_TIME)) { set_pwm(HEAD_AC220_INDEX); }
		if (!((ctr+19) % EVERY_NTH_TIME)) { set_pwm(HOOTER_INDEX); }
	}
	//slow_timer_callback_unlock_and_restart:
	spin_unlock(&slow_timer_callback_lock);
	return HRTIMER_RESTART;
}

static enum hrtimer_restart sloow_timer_callback (struct hrtimer *timer) 
{
	ktime_t ktime, now;
	uint64_t overrun;
	int diff;
	static int blink_timer = 0;
	static bool on_cycle = false;
	ktime = ktime_set(0,laarge_delay);

	//printk(KERN_INFO "%s: in %s.", MODNAME, __FUNCTION__);

	now = hrtimer_cb_get_time(timer);
	overrun = hrtimer_forward(timer, now, ktime);

	if (!spin_trylock(&sloow_timer_callback_lock)) 
	{
		printk(KERN_INFO "%s: %s didnt get lock. restarting timer.\n", MODNAME, __FUNCTION__);
		return HRTIMER_RESTART;
	}
	//do all sloow works:
	read_door_latch();

	//housekeeping: blink LED pattern
	if (on_cycle) blink_timer +=  laarge_delay_ms;
	else blink_timer -=  laarge_delay_ms;

	if (blink_timer >= BLINK_TIME_MS) { blink_timer = BLINK_TIME_MS; on_cycle = false;}
	if (blink_timer <= 0) {blink_timer = 0; on_cycle = true;}

	switch (rgb.pattern)
	{
		case LIGHT_OFF:
			schedule_set_pwm (RGB_R_INDEX, 0);
			schedule_set_pwm (RGB_G_INDEX, 0);
			schedule_set_pwm (RGB_B_INDEX, 0);
			break;
		case LIGHT_ON:
			schedule_set_pwm (RGB_R_INDEX, rgb.red);
			schedule_set_pwm (RGB_G_INDEX, rgb.green);
			schedule_set_pwm (RGB_B_INDEX, rgb.blue);
			break;
		case LIGHT_BLINK:
			if (on_cycle)
			{
				schedule_set_pwm (RGB_R_INDEX, rgb.red);
				schedule_set_pwm (RGB_G_INDEX, rgb.green);
				schedule_set_pwm (RGB_B_INDEX, rgb.blue);
			}
			else
			{
				schedule_set_pwm (RGB_R_INDEX, 0);
				schedule_set_pwm (RGB_G_INDEX, 0);
				schedule_set_pwm (RGB_B_INDEX, 0);
			}
			break;
		case LIGHT_HEARTBEAT:
			schedule_set_pwm (RGB_R_INDEX, (blink_timer / BLINK_TIME_MS) * rgb.red);
			schedule_set_pwm (RGB_G_INDEX, (blink_timer / BLINK_TIME_MS) * rgb.green);
			schedule_set_pwm (RGB_B_INDEX, (blink_timer / BLINK_TIME_MS) * rgb.blue);
			break;
	}

	//housekeeping: if too many hardware errors, disable everything and raise error
	if (iostatus.watchdog_timer > 0)
	{
		iostatus.watchdog_timer -= laarge_delay_ms;
		if (iostatus.watchdog_timer <= 0)
		{
			printk("%s: %s - watchdog timeout!\n", MODNAME, __FUNCTION__);
			iostatus.watchdog_timer = -1;//so that it doesn't bite again after reset till we want it to.
			handle_hw_error();
			iostatus.watchdog_timeout_error = true;
		}
	}

	diff = (module_stats.__old_i2c_errors) - (module_stats.i2c_errors); if (diff < 0) diff = (-1) * diff;
	if (diff > TOOMANY_I2C_ERRORS)
	{
		iostatus.i2c_hardware_error = true;
		handle_hw_error();

		printk("%s: %s - too many i2c hardware errors.\n", MODNAME, __FUNCTION__);
	}

	else module_stats.__old_i2c_errors = module_stats.i2c_errors;

	diff = (module_stats.__old_spi0_errors) - (module_stats.spi0_errors); if (diff < 0) diff = (-1) * diff;
	if (diff > TOOMANY_SPI0_ERRORS)
	{
		handle_hw_error();
		iostatus.spi0_hardware_error = true;
		printk("%s: %s - too many spi0 hardware errors.\n", MODNAME, __FUNCTION__);
	}
	else module_stats.__old_spi0_errors = module_stats.spi0_errors;

	diff = (module_stats.__old_spi1_errors) - (module_stats.spi1_errors); if (diff < 0) diff = (-1) * diff;
	if (diff > TOOMANY_SPI1_ERRORS)
	{
		handle_hw_error();
		iostatus.spi1_hardware_error = true;
		printk("%s: %s - too many spi1 hardware errors.\n", MODNAME, __FUNCTION__);
	}
	else module_stats.__old_spi1_errors = module_stats.spi1_errors;

	spin_unlock(&sloow_timer_callback_lock);
	return HRTIMER_RESTART;
}

int data_getter_init (void) 
{
	ktime_t ktime;
	small_delay = US_100; large_delay=US_500; laarge_delay=MS_5; laarge_delay_ms = 5, tprof=0; 
	//small_delay = MS_1; large_delay=MS_10; laarge_delay=MS_100; laarge_delay_ms=100, tprof=1; 
	small_delay = US_200; large_delay=US_600; laarge_delay=MS_5; laarge_delay_ms=5, tprof=2; 
	printk (KERN_INFO "%s: %s profile new1=%ld\n", MODNAME, __FUNCTION__, tprof);

	gCannot_continue = false;
	if (initialize_hardware() != 0) return -1;
	if (initialize_values() != 0) return -1;
	if (create_proc_entries() != 0) return -1; 

	allow_signal(SIGKILL);

	hrtimer_init(&fast_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_init(&slow_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_init(&sloow_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	fast_timer.function = &fast_timer_callback;
	slow_timer.function = &slow_timer_callback;
	sloow_timer.function = &sloow_timer_callback;

	ktime = ktime_set(0,small_delay); hrtimer_start(&fast_timer, ktime, HRTIMER_MODE_REL);
	ktime = ktime_set(0,large_delay); hrtimer_start(&slow_timer, ktime, HRTIMER_MODE_REL);
	ktime = ktime_set(0,laarge_delay); hrtimer_start(&sloow_timer, ktime, HRTIMER_MODE_REL);

	return 0;
}

void data_getter_cleanup(void) 
{
	printk(KERN_INFO "%s: in %s", MODNAME, __FUNCTION__);
	turn_off_everything();
	remove_proc_entries();


	//Reset
	hrtimer_cancel(&fast_timer);
	hrtimer_cancel(&slow_timer);
	hrtimer_cancel(&sloow_timer);
}

MODULE_LICENSE("GPL"); 
module_init(data_getter_init);
module_exit(data_getter_cleanup);
