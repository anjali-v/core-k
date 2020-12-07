#ifndef __DATA_GETTER_H
#define __DATA_GETTER_H

// Proc related
#define PROC "/proc/sense_os/"
//#define PROC "/proc/"
#define PROC_DIRNAME "sense_os"
#define PROC_RTD_FILENAME "rtd"
#define PROC_COUNTERS_FILENAME "counters" 
//PWM
#define PROC_HEATER_FILENAME "heaters"
#define PROC_RGB_FILENAME "rgb"
#define PROC_HOOTER_FILENAME "hooter"
#define PROC_HEAD_AC220_FILENAME "head_ac220"
//Rpi GPIO
#define PROC_ESTOP_FILENAME "estop"
#define PROC_POWERCUT_FILENAME "powercut"
//GPIOx
#define PROC_AXIS_FILENAME "axis"
#define PROC_DOOR_FILENAME "door"
#define PROC_MICROSTEP_FILENAME "microstep"
#define PROC_RFID0_FILENAME "rfid0"
#define PROC_RFID1_FILENAME "rfid1"
//software
#define PROC_IOSTATUS_FILENAME "iostatus"
#define PROC_RESETHW_FILENAME "reset_hw"
#define PROC_RESETRTD_FILENAME "reset_rtd"
#define PROC_ENABLEHW_FILENAME "enable_hw"
#define PROC_STATISTICS_FILENAME "statistics"
#define PROC_TESTMODE_FILENAME "testmode"

#define PROC_WATCHDOG_TIMEOUT_FILENAME "watchdog_timeout_msec"

//interface health
#define TOOMANY_I2C_ERRORS 1000
#define TOOMANY_SPI0_ERRORS 10
#define TOOMANY_SPI1_ERRORS 1

typedef enum {FRONT_DOOR=0, FILAMENT_DOOR, TOP_DOOR, BAKER_DOOR} door_t;
typedef enum {OPEN_DOOR=0, CLOSE_DOOR} dooroperation_t;
typedef enum {DOOR_OPEN=0, DOOR_CLOSED} doorstatus_t;
typedef enum {X_AXIS=0, Y_AXIS, Z_AXIS} axis_t;
typedef enum {ESTOP_HIT=0, ESTOP_CLEAR} estop_t;
typedef enum {POWER_PRESENT=0, POWER_GONE} powercut_t;
typedef bool reset_t;
typedef bool reset_rtd_t;
typedef bool enable_t;
typedef bool testmode_t;

//used for axis enable
typedef struct
{
	axis_t axis;
	bool enable;
} axisdata_t;

//used for microstep enable
typedef struct
{
	axis_t axis;
	uint8_t value;
} microstep_t;


//used to set door enable
typedef struct
{
	door_t door;
	dooroperation_t value;
} doordata_t;

//used to get all statuses
typedef struct
{
	estop_t estop;
	powercut_t powercut;
	bool is_enabled;
	bool i2c_hardware_error;
	bool spi0_hardware_error;
	bool spi1_hardware_error;
	bool watchdog_timeout_error;
	bool hardware_error;
	bool xfault;
	bool yfault;
	bool zfault;
	bool xmin_hit;
	bool xmax_hit;
	bool ymin_hit;
	bool ymax_hit;
	bool zmin_hit;
	bool zmax_hit;
	doorstatus_t front_door_status;
	doorstatus_t filament_door_status;
	doorstatus_t top_door_status;
	doorstatus_t baker_door_status;
	reset_t reset_status;
	reset_rtd_t reset_i2c0;
	testmode_t testmode;
	enable_t enabled_status;
	int watchdog_timer;
} iostatus_t;

typedef struct
{
	uint32_t i2c_transfers;
	uint32_t spi0_transfers;
	uint32_t spi1_transfers;
	uint32_t i2c_errors;
	uint32_t spi0_errors;
	uint32_t spi1_errors;
	uint32_t i2c0_reset_count;
	uint32_t pwm_update_missed;
	uint32_t copy_from_user_errors;
	uint32_t __old_i2c_errors;
	uint32_t __old_spi0_errors;
	uint32_t __old_spi1_errors;
	char last_err_msg[100];
} stats_t;

typedef struct
{
	uint8_t vendor_id; //8 bits vendorname, material name
	uint8_t Material_id; //all materials possible from that vendor
	uint8_t batchno; //batch no which can give 
	uint8_t mfg_dd; 
	uint8_t mfg_mm; 
	uint8_t mfg_yy; 
	uint32_t total_len_mm;
	uint32_t remaining_len_mm;
} rfiddata_t;

typedef bool door_latch_t;

// raspi GPIO related
#define ESTOP_GPIO 12
#define POWERCUT_GPIO 7

// RTD related
#define RTD_XFERLEN 9
#define RTD_SPISPEED 2000000
#define NUM_RTDS 5

typedef struct 
{
	char buf[NUM_RTDS][RTD_XFERLEN];
} rtdbuf_t;

// SPI1 (uc counters) related
#define NUM_COUNTERS 5
typedef union
{
	int32_t data32;
	uint8_t  data8[4];
} spicounter_t;

typedef struct __attribute__((__packed__)) spi_buf
{
	uint8_t cksum;
	spicounter_t counter[NUM_COUNTERS];
} spibuf_t;

// PWM chip (heater, rgbled) related
#define PWM_CHIP 0x40

#define PWM_MAX 4096

#define PCA9685_ADDRESS    0x40
#define MODE1              0x00
#define MODE2              0x01
#define SUBADR1            0x02
#define SUBADR2            0x03
#define SUBADR3            0x04
#define PRESCALE           0xFE
#define LED0_ON_L          0x06
#define LED0_ON_H          0x07
#define LED0_OFF_L         0x08
#define LED0_OFF_H         0x09
#define ALL_LED_ON_L       0xFA
#define ALL_LED_ON_H       0xFB
#define ALL_LED_OFF_L      0xFC
#define ALL_LED_OFF_H      0xFD

// Bits:
#define PWM_RESTART        0x80
#define PWM_SLEEP          0x10
#define PWM_ALLCALL        0x01
#define PWM_INVRT          0x10
#define PWM_OUTDRV         0x04

#define NUM_PWM_CHANNELS 9
#define NUM_HEATERS 4

#define RGB_R_INDEX 0
#define RGB_G_INDEX 1
#define RGB_B_INDEX 2
#define HEATER_INDEX 3
#define HEAD_AC220_INDEX 7
#define HOOTER_INDEX 8

typedef enum {HOOTER_OFF=0, HOOTER_ON}  hooter_t;
typedef enum {HEAD_AC220_OFF=0, HEAD_AC220_ON}  head_ac220_t;

typedef enum {BED_HEATER=0,BUILD_CHAMBER_LEFT_HEATER,BUILD_CHAMBER_RIGHT_HEATER,BAKING_CHAMBER_HEATER} heater_t;

typedef struct
{
	uint16_t value[NUM_HEATERS];
} heaterdata_t;
//
// RGB LED related
#define BLINK_TIME_MS 500
typedef enum {OFF=0, RED, GREEN, BLUE, MAGENTA, CYAN, WHITE } colour_t;
typedef enum {LIGHT_OFF=0, LIGHT_ON, LIGHT_BLINK, LIGHT_HEARTBEAT} pattern_t; 
typedef struct
{
	uint16_t red;
	uint16_t green;
	uint16_t blue;
	pattern_t pattern;
} rgbdata_t;

//////// UDP messages /////////////
typedef enum {SENSEOS_FS_SLOW=0, SENSEOS_FS_FAST, SERIAL_DATA, SENSEOS_WRITE,RPI_ERROR}  udp_msg_t;
typedef struct
{
	rtdbuf_t rx_rtd;
	stats_t stats;
	iostatus_t iostatus;

}udp_senseos_slow_t;

typedef struct
{
	spibuf_t readspi;
}udp_senseos_fast_t;

typedef struct 
{
	udp_msg_t msgtype;
	union 
	{
		udp_senseos_slow_t slow_data;
		udp_senseos_fast_t fast_data;
		char serial_data[100];
	} u;
}udp_sendto_pc_t;

typedef struct
{

	axisdata_t en_axis;
        microstep_t set_microstep;
        doordata_t set_door;
        heaterdata_t set_heater;
        rgbdata_t set_rgb;
        hooter_t set_hooter;
        head_ac220_t set_head;
        enable_t set_en;
        reset_t rst;
}udp_senseos_write_t;

typedef struct
{
	udp_msg_t msgtype;
	union
	{
		udp_senseos_write_t write_data;
		char serial_data[100];
	}u;
	
}udp_recivefrm_pc_t;

// GPIO expander related
#define GPIOX1 0x20
#define GPIOX2 0x24

typedef enum {PORT_A, PORT_B} port_t;

typedef struct __attribute__((__packed__)) 
{
    uint8_t x_fault: 1;
    uint8_t y_fault: 1;
    uint8_t z_fault: 1;
    uint8_t rtd1: 1;
    uint8_t rtd2: 1;
    uint8_t rtd3: 1;
    uint8_t rtd4: 1;
    uint8_t rtd5: 1;
} gpiox1a_bitfield_t;

typedef struct __attribute__((__packed__)) 
{
    uint8_t x_min: 1;
    uint8_t x_max: 1;
    uint8_t y_min: 1;
    uint8_t y_max: 1;
    uint8_t z_min: 1;
    uint8_t z_max: 1;
    uint8_t rfid0_cs: 1;
    uint8_t rfid1_cs: 1;
} gpiox1b_bitfield_t;

typedef struct __attribute__((__packed__)) 
{
    uint8_t close_door: 1;
    uint8_t door_status: 1;
    uint8_t x_enable: 1;
    uint8_t y_enable: 1;
    uint8_t z_enable: 1;
    uint8_t microstep_x:3; 
} gpiox2a_bitfield_t;

typedef struct __attribute__((__packed__)) 
{
    uint8_t microstep_y:3; 
    uint8_t microstep_z:3; 
    uint8_t reset_rfid0: 1;
    uint8_t reset_rfid1: 1;
} gpiox2b_bitfield_t;

typedef union
{
    uint8_t val;
    gpiox2a_bitfield_t p2a;
    gpiox2b_bitfield_t p2b;
    gpiox1a_bitfield_t p1a;
    gpiox1b_bitfield_t p1b;
} bitfields_t;


#endif // __DATA_GETTER_H
