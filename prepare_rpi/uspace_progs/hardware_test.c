#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <wiringPi.h>
#include <errno.h>
#include <stdint.h>
#include <wiringSerial.h>
#include <ctype.h>

#include <data_getter.h>

#define NUM_RTDS 5
#define RTD_XFERLEN 9
#define RREF 430

#define X_STP 0     //17
#define Y_STP 3     //22
#define Z_STP 22    //6
#define A_STP 25    //26
#define B_STP 10    //8

#define X_DIR 7     //4
#define Y_DIR 2     //27
#define Z_DIR 21    //5
#define A_DIR 23     //13
#define B_DIR 6    //25

int fd,ret;
char *mystr="";
int pwm,m_en,m1,m2,m3;
char head;

int read_from_port();
int write_to_port(char*);
void handle_data();
uint8_t calc_crc8(char*);
int microstep(char *,char *,char *);



int microstep(char *m1,char *m2,char *m3)
{
	int M1,M2,M3;
	M1=atoi(m1)<<2;
	M2=atoi(m2)<<1;
	M3=atoi(m3)<<0;
	int microstep=M1|M2|M3;
	return microstep;
}



uint8_t calc_crc8(char *data)
{
	uint8_t crc=0xff,crc1;
	int i,j;
	int len=strlen(data);
	for(i=5;i<len-1;i++)
	{
		crc^=data[i];
		for(j=0;j<8;j++)
		{if((crc&0x80) !=0)
			crc=(uint8_t)((crc<<1)^0x31);
			else
				crc <<=1;}
	}
	return crc;
}

int read_from_port()
{

	while(1)
	{
		char rd_c ;
		if(fd>0)
		{
			rd_c=serialGetchar (fd);
			ret = asprintf(&mystr, "%s%c", mystr, rd_c);
			if(rd_c=='Z')
			{handle_data(mystr);mystr="";}
		}
		
	}
}
int test_read_from_port()
{
int i=0;
		char rd_c ;
		while(i<=2)
		{
			rd_c=serialGetchar (fd);
			ret = asprintf(&mystr, "%s%c", mystr, rd_c);
			if(rd_c=='Z')
			{handle_data(mystr);mystr="";i++;}
		}
}
int write_to_port(char *str)
{
	while(1)
	{
		printf("\nstr - %s",str);
		for(int i=0;i<strlen(str);i++)
			serialPutchar (fd,str[i]) ;
	}
}
int test_write_to_port(char *str)
{

		printf("\nstr - %s",str);
		for(int i=0;i<strlen(str);i++)
			serialPutchar (fd,str[i]) ;
}

void handle_data(char *input)
{
	int i=0,x;
	int crc=calc_crc8(input);
	char *new[100],*crc1;
	asprintf(&crc1,"%x",crc);

	//UPPER CASE
	while(i<5)
	{crc1[i]=(toupper(crc1[i]));i++;}

	//SPLIT
	char *token = strtok(input,",");
	for(i=0;i<strlen(input),(token);i++)
	{   new[i]=token;
		token = strtok(NULL, ","); }

	//COMPARE
	if(strcmp(new[0]+2,crc1)==0)
	{
		//      printf("\n new[1]=%s len1=%d",new[2],strlen(new[2]));
		if(strcmp(new[1],"X")==0)
		{
			if(strcmp(new[2],"A")==0)
			{
				printf("\n---------HEAD_A----------------");
				printf("\nRTD A1 :              %s",new[3]);
				printf("\nRTD A2 :              %s",new[4]);
				printf("\nFilament Pressure A:  %s",new[5]);
				printf("\nActual PWM A:         %s",new[6]);
				printf("\nMotor Enable A :      %s",new[7]);
				printf("\nFilament Status A :   %s",new[8]);
				printf("\nHead Fault A :        %s",new[9]);
				printf("\nHead ID A :           %s",new[10]);
				printf("\nMicrostep_A           %d",microstep(new[11],new[12],new[13]));
			}
			else if(strcmp(new[2],"B")==0)
			{
				printf("\n---------HEAD_B----------------");
				printf("\nRTD B1 :              %s",new[3]);
				printf("\nRTD B2 :              %s",new[4]);
				printf("\nFilament Pressure B:  %s",new[5]);
				printf("\nActual PWM B:         %s",new[6]);
				printf("\nMotor Enable B:       %s",new[7]);
				printf("\nFilament Status B:    %s",new[8]);
				printf("\nHead Fault B:         %s",new[9]);
				printf("\nHead ID B:            %s",new[10]);
				printf("\nMicrostep_B           %d",microstep(new[11],new[12],new[13]));
			}
		}       }

}



void init_gpio()
{
	pinMode (X_STP, OUTPUT) ;
	pinMode (X_DIR, OUTPUT) ;
	pinMode (Y_STP, OUTPUT) ;
	pinMode (Y_DIR, OUTPUT) ;
	pinMode (Z_STP, OUTPUT) ;
	pinMode (Z_DIR, OUTPUT) ;
	pinMode (A_STP, OUTPUT) ;
	pinMode (A_DIR, OUTPUT) ;
	pinMode (B_STP, OUTPUT) ;
	pinMode (B_DIR, OUTPUT) ;
}

void set_dir(int dir,int state)
{
	digitalWrite (dir, state) ;       // Off
}
void set_steps(int stp)
{
	digitalWrite (stp, 1) ;       // On
	usleep(100);
	digitalWrite (stp, 0) ;       // Off
	usleep(100);
}
void run(int dir,int stp)
{
	int state=0,i=0;
	while(1)
	{
		set_dir(dir,state);
		set_steps(stp);
		i++;
		if(i>5000)
		{state=~state;
			i=0;}
	}
}
void test_run(int dir,int stp)
{
	int state=0,i=0;
	while(i<10000)
	{
		set_dir(dir,state);
		set_steps(stp);
		i++;
		if(i%5000==0)
		{state=~state;}
	}
}



static float callendar_van_dusen(float r)
{
	float a = 3.9083E-03;
	float b = -5.7750E-07;
	//int32_t r0 = RREF/4;
	float r0 = 100.0;

	return((((0 - r0) * a) + (sqrt((r0 * r0 * a * a) - (4 * r0 * b * (r0-r)))))/(2 * r0 * b));
}

static float read_temp(uint8_t *rx)
{
	/*
	 * CONFIGURATION
	 * Rref = 400  # Rref = 400 for PT100, Rref = 4000 for PT1000
	 * wire = 2    # PT100/1000 has 2 or 3 or 4 wire connection 
	 */

	uint32_t rtd_data = 0;
	uint32_t adc_code = 0;
	float r = 0, j=0,val=32768.0;
	float temp_degree = 0;

	/*    printf("rx= ");
	      for (j=0; j < RTD_XFERLEN; j++) printf("%d ", rx[j]);
	      printf("\n");
	      */
	rtd_data = (rx[2] << 8) | rx[3];
	adc_code = rtd_data >> 1;
	r = (adc_code * RREF)/val;
	temp_degree = callendar_van_dusen(r);

	//printf("rtd_data=0x%x, adc_code-0x%x, r=%f temprature=%f\n",rtd_data, adc_code, r, temp_degree);
	//printf("t=%3.2f ", temp_degree);

	return(temp_degree);
}

void test_rtd(void)
{	
	char ctr=0;
	char rx_rtdbuf[NUM_RTDS][RTD_XFERLEN];
	int i, j;
	//FILE *fp = fopen ("/proc/sense_os/rtd", "rb");
	FILE *fp = fopen (PROC PROC_RTD_FILENAME, "rb");
		usleep(500000);
		fread (rx_rtdbuf, sizeof(rx_rtdbuf), 1, fp);
		ctr++;
		printf ("rtd data\n");
		for (i=0; i<NUM_RTDS; i++)
		{
			for (j=0; j<RTD_XFERLEN; j++);
			//	printf ("%02x ", rx_rtdbuf[i][j]);
			//printf ("\n");
		}

		for (i=0; i<NUM_RTDS; i++)
		{
			read_temp(rx_rtdbuf[i]);
			printf ("temp%d=%3.2f ", i, read_temp(rx_rtdbuf[i]));
		}

		printf ("\n%d \n", ctr);
}
void rtd(void)
{	
	char ctr=0;
	char rx_rtdbuf[NUM_RTDS][RTD_XFERLEN];
	int i, j;
	//FILE *fp = fopen ("/proc/sense_os/rtd", "rb");
	FILE *fp = fopen (PROC PROC_RTD_FILENAME, "rb");
	while (true)
	{
		usleep(500000);
		fread (rx_rtdbuf, sizeof(rx_rtdbuf), 1, fp);
		ctr++;
		printf ("rtd data\n");
		for (i=0; i<NUM_RTDS; i++)
		{
			for (j=0; j<RTD_XFERLEN; j++)
				printf ("%02x ", rx_rtdbuf[i][j]);
			printf ("\n");
		}

		for (i=0; i<NUM_RTDS; i++)
		{
			read_temp(rx_rtdbuf[i]);
			//printf ("temp[%d]=%3.2f ", i, read_temp(rx_rtdbuf[i]));
		}

		printf ("\n%d \n", ctr);
	}
}

void test_counters() 
{
	char ctr;
	int i, j;
	spibuf_t buf;
	FILE *fp;
	struct timeval st, et;
	fp = fopen ("/proc/sense_os/counters", "rb");		
		gettimeofday(&st,NULL);
		fread (&buf, sizeof(buf), 1, fp);
		gettimeofday(&et,NULL);
		ctr++;
		printf ("\e[?25lcrc: %02x ", buf.cksum);
		for (i=0; i<NUM_COUNTERS; i++)
			printf ("c%d: %08x ", i, buf.counter[i]);
		int elapsed = ((et.tv_sec - st.tv_sec) * 1000000) + (et.tv_usec - st.tv_usec);
		printf("%03dus\r",elapsed);
		usleep(5000);
}
void counters() 
{
	char ctr;
	int i, j;
	spibuf_t buf;
	FILE *fp;
	struct timeval st, et;
	fp = fopen ("/proc/sense_os/counters", "rb");
	while (1)		
	{
		gettimeofday(&st,NULL);
		fread (&buf, sizeof(buf), 1, fp);
		gettimeofday(&et,NULL);
		ctr++;
		printf ("\e[?25lcrc: %02x ", buf.cksum);
		for (i=0; i<NUM_COUNTERS; i++)
			printf ("c%d: %08x ", i, buf.counter[i]);
		int elapsed = ((et.tv_sec - st.tv_sec) * 1000000) + (et.tv_usec - st.tv_usec);
		printf("%03dus\r",elapsed);
		usleep(5000);
	}
}

void test_gpios() 
{
	char ctr=0;
	int i, j, ret;
	FILE *fp ;
	iostatus_t buf;
		usleep(500000);
		fp=fopen ("/proc/sense_os/iostatus", "rb");
		ret = fread (&buf, 1, sizeof(buf), fp);
		printf ("ret=%d\n", ret);
		ctr++;
		printf ("estop:    %d\n", buf.estop);
		printf ("powercut: %d\n", buf.powercut);
		printf ("i2c_err:  %d\n", buf.i2c_hardware_error);
		printf ("spi0_err: %d\n", buf.spi0_hardware_error);
		printf ("spi1_err: %d\n", buf.spi1_hardware_error);
		printf ("hwerror:  %d\n", buf.hardware_error);
		printf ("is_enbld: %d\n", buf.is_enabled);
		printf ("reset:    %d\n", buf.reset_status);
		printf ("enable:   %d\n", buf.enabled_status);
		printf ("xfault:   %d\n", buf.xfault);
		printf ("yfault:   %d\n", buf.yfault);
		printf ("zfault:   %d\n", buf.zfault);
		printf ("xmin_hit: %d\n", buf.xmin_hit);
		printf ("xmax_hit: %d\n", buf.xmax_hit);
		printf ("ymin_hit: %d\n", buf.ymin_hit);
		printf ("ymax_hit: %d\n", buf.ymax_hit);
		printf ("zmin_hit: %d\n", buf.zmin_hit);
		printf ("zmax_hit: %d\n", buf.zmax_hit);
		printf ("door:     %d\n", buf.front_door_status);
		printf ("%d\n", ctr);
}
void gpios() 
{
	char ctr=0;
	int i, j, ret;
	FILE *fp ;
	iostatus_t buf;
	while (true)
	{
		usleep(500000);
		fp=fopen ("/proc/sense_os/iostatus", "rb");
		ret = fread (&buf, 1, sizeof(buf), fp);
		printf ("ret=%d\n", ret);
		ctr++;
		printf ("estop:    %d\n", buf.estop);
		printf ("powercut: %d\n", buf.powercut);
		printf ("i2c_err:  %d\n", buf.i2c_hardware_error);
		printf ("spi0_err: %d\n", buf.spi0_hardware_error);
		printf ("spi1_err: %d\n", buf.spi1_hardware_error);
		printf ("hwerror:  %d\n", buf.hardware_error);
		printf ("is_enbld: %d\n", buf.is_enabled);
		printf ("reset:    %d\n", buf.reset_status);
		printf ("enable:   %d\n", buf.enabled_status);
		printf ("xfault:   %d\n", buf.xfault);
		printf ("yfault:   %d\n", buf.yfault);
		printf ("zfault:   %d\n", buf.zfault);
		printf ("xmin_hit: %d\n", buf.xmin_hit);
		printf ("xmax_hit: %d\n", buf.xmax_hit);
		printf ("ymin_hit: %d\n", buf.ymin_hit);
		printf ("ymax_hit: %d\n", buf.ymax_hit);
		printf ("zmin_hit: %d\n", buf.zmin_hit);
		printf ("zmax_hit: %d\n", buf.zmax_hit);
		printf ("door:     %d\n", buf.front_door_status);
		printf ("%d\n", ctr);
	}
}

void set_microstep(axis_t axis, uint8_t value)
{
	char ctr;
	int i, j;
	FILE *fp = fopen ("/proc/sense_os/microstep", "wb");
	if (!fp) printf ("marega aage. sudo se chala ke dekhna agli baar.\n");

	microstep_t buf;
	buf.axis = axis;
	buf.value = value;
	fwrite (&buf, sizeof(buf), 1, fp);
	fclose(fp);
}

void set_axis(axis_t axis, bool enable)
{
	char ctr;
	int i, j;
	FILE *fp = fopen ("/proc/sense_os/axis", "wb");
	if (!fp) printf ("marega aage. sudo se chala ke dekhna agli baar.\n");
	axisdata_t buf;
	buf.axis = axis;
	buf.enable = enable;
	fwrite (&buf, sizeof(buf),1,  fp);
	fclose(fp);
}

void set_heaters(uint16_t v0, uint16_t v1, uint16_t v2, uint16_t v3)
{
	char ctr;
	int i, j;
	FILE *fp = fopen ("/proc/sense_os/heaters", "wb");
	if (!fp) printf ("marega aage. sudo se chala ke dekhna agli baar.\n");
	heaterdata_t buf;
	buf.value[0] = v0;
	buf.value[1] = v1;
	buf.value[2] = v2;
	buf.value[3] = v3;
	fwrite (&buf, sizeof(buf),1,  fp);
}
void set_hooter(bool hooter)
{
	char ctr;
	int i, j;
	FILE *fp = fopen ("/proc/sense_os/hooter", "wb");
	if (!fp) printf ("marega aage. sudo se chala ke dekhna agli baar.\n");
	hooter_t buf=hooter;
	fwrite (&buf, sizeof(buf),1,  fp);
}

void set_reset(reset_t rst)
{
	char ctr;
	FILE *fp = fopen ("/proc/sense_os/reset_hw", "wb");
	if (!fp) printf ("marega aage. sudo se chala ke dekhna agli baar.\n");
	reset_t buf=rst;
	fwrite (&buf, sizeof(buf),1,  fp);
}
void set_resetrtd(reset_rtd_t rst)
{
	char ctr;
	FILE *fp = fopen ("/proc/sense_os/reset_rtd_hw", "wb");
	if (!fp) printf ("marega aage. sudo se chala ke dekhna agli baar.\n");
	reset_rtd_t buf=rst;
	fwrite (&buf, sizeof(buf),1,  fp);
}
void set_testmode(testmode_t rst)
{
	char ctr;
	FILE *fp = fopen ("/proc/sense_os/testmode", "wb");
	if (!fp) printf ("marega aage. sudo se chala ke dekhna agli baar.\n");
	testmode_t buf=rst;
	fwrite (&buf, sizeof(buf),1,  fp);
}

void set_enable(bool en)
{
	char ctr;
	FILE *fp = fopen ("/proc/sense_os/enable_hw", "wb");
	if (!fp) printf ("marega aage. sudo se chala ke dekhna agli baar.\n");
	enable_t buf=en;
	fwrite (&buf, sizeof(buf),1,  fp);

}

void set_enable_220v(bool en)
{
	char ctr;
	FILE *fp = fopen ("/proc/sense_os/head_ac220", "wb");
	if (!fp) printf ("marega aage. sudo se chala ke dekhna agli baar.\n");
	head_ac220_t buf=en;
	fwrite (&buf, sizeof(buf),1,  fp);
}
void set_rgb(uint16_t red, uint16_t green,uint16_t blue)
{
	char ctr;
	int i, j;
	FILE *fp = fopen ("/proc/sense_os/rgb", "wb");
	if (!fp) printf ("marega aage. sudo se chala ke dekhna agli baar.\n");
	rgbdata_t buf;
	buf.red =red ;
	buf.green = green;
	buf.blue = blue;
	buf.pattern=1;
	fwrite (&buf, sizeof(buf),1,  fp);
}

void set_door(int door,bool value)
{
	char ctr;
	int i, j;
	doordata_t buf;
	FILE *fp = fopen ("/proc/sense_os/door", "wb");
	if (!fp) printf ("marega aage. sudo se chala ke dekhna agli baar.\n");
	buf.door=door;
	buf.value=value;
	fwrite (&buf, sizeof(buf),1,  fp);
}

void see_statistic()
{ 
	char ctr; 
	int i, j;
	stats_t buf;
	FILE *fp;
	while(1)
	{
		usleep(500000);
		fp=fopen ("/proc/sense_os/statistics", "rb");
		fread (&buf, sizeof(buf), 1, fp);
		ctr++;
		printf ("i2c_xfers:   %20ld\n", buf.i2c_transfers);
		printf ("spi0_xfers:  %20ld\n", buf.spi0_transfers);
		printf ("spi1_xfers:  %20ld\n", buf.spi1_transfers);
		printf ("i2c_errors:  %03d\n", buf.i2c_errors);
		printf ("spi0_errors: %03d\n", buf.spi0_errors);
		printf ("spi1_errors: %03d\n", buf.spi1_errors);
		printf ("pwm_upd_msd: %03d\n", buf.pwm_update_missed);
		printf ("c_f_u_errs:  %03d\n", buf.copy_from_user_errors);
		printf ("last_err_msg: ");
		puts(buf.last_err_msg);
		printf("%d \n",ctr);
	}
}
/*
   void rfid()
   {
   char ctr;
   int i, j;
   FILE *fp = fopen ("/proc/sense_os/iostatus", "wb");
   rgb_t buf;
   }
   */

int main()
{
	FILE *fp;
	int choice;

	int axis,value,v0,v1,v2,v3,num,red,green,blue,door,mvaxis,microstep;
	bool enable,hooter,valued,en,en_220,rst,rst_rtd=1,test=1;
	char str[100];
	if ((fd = serialOpen ("/dev/ttyAMA0", 115200)) < 0)
	{
		fprintf (stderr, "Unable to open serial device: %s\n", strerror (errno)) ;
		return 1 ;
	}


	if (wiringPiSetup () == -1)
	{   printf("wiringpi ERROR");
		return 1 ;}
	init_gpio();

	printf ("1: Reset\n");
	printf ("2: Enable\n");
	printf ("3: see RTDs\n");
	printf ("4: see counters\n");
	printf ("5: Set microstep\n");
	printf ("6: Set motor_enable\n");
	printf ("7: Set heaters\n");
	printf ("8: Set hooter\n");
	printf ("9: Set rgb\n");
	printf ("10: Set door\n");
	printf ("11: Enable 220\n");
	printf ("12: See stats\n");
	printf ("13: See status pins\n");
	printf ("14: Move Axis\n");
	printf ("15: Read from Port\n");
	printf ("16: Write to port\n");
	printf ("17: test_rtd\n");
	printf ("18: test_counter\n");
	printf ("19: test_gpios\n");
	printf ("20: test run\n");
	printf ("21: test_read_from_port\n");
	printf ("22: test_write_to port\n");
	printf ("23: rst_rtd\n");
	printf ("24: testmode\n");
	scanf ("%d", &choice);

	switch (choice)
	{
		case 1:
			printf ("RESET (0/1) ? ");scanf ("%d", &rst);
			set_reset(rst);
			break;
		case 2:
			printf ("ENABLE (0/1) ? ");scanf ("%d", &en);
			set_enable(en);
			break;
		case 3:
			rtd();
			break;
		case 4:
			counters();
			break;
		case 5:
			printf ("Which axis? ( X=0, Y=1, Z=2 ): ");scanf ("%d", &axis);
			if (axis > 2) {printf ("bad axis. Bye.\n"); exit(0);}
			printf ("What value? ( 0 to 7 ): "); scanf ("%d", &value);
			if (value > 7) {printf ("bad value. Bye.\n"); exit(0);}

			set_microstep(axis, value);
			break;
		case 6:
			printf ("Which axis? ( X=0, Y=1, Z=2 ): ");scanf ("%d", &axis);
			if (axis > 2) {printf ("bad axis. Bye.\n"); exit(0);}
			printf ("select to: enable=1 and disable=0 "); scanf ("%d", &enable);
			set_axis(axis,enable);
			break;
		case 7:
			printf ("What four heater values? ( 0 to 4095 ): "); scanf ("%d%d%d%d", &v0, &v1, &v2, &v3);
			set_heaters(v0,v1,v2,v3);
			break;
		case 8:
			printf ("HOOTER  (ON=0, OFF=1) ");scanf ("%d", &hooter);
			set_hooter(hooter);
			break;
		case 9:
			printf ("R G B? : ");scanf ("%d%d%d", &red,&green,&blue);
			if(red>4095||green>4095||blue>4095)
			{printf("bad value");exit(0);}
			set_rgb(red,green,blue);
			break;
		case 10:
			printf ("FRONT_DOOR=0, FILAMENT_DOOR=1, TOP_DOOR=2, BAKER_DOOR=3  :   ");scanf ("%d", &door);
			if (door>3) {printf ("bad choice\n"); exit(0);}
			printf("DOOR_OPEN=0, DOOR_CLOSED=1  :  ");scanf("%d",&valued);
			set_door(door,valued);
			break;
		case 11:
			printf ("ENABLE 220v  (ON=0, OFF=1) ");scanf ("%d", &en_220);
			set_enable_220v(en_220);
			break;
		case 12:
			see_statistic();
			break;
		case 13:
			gpios();
			break;
		case 14:
			printf ("Which axis? ( X=0, Y=1, Z=2, A=3, B=4 ): ");scanf ("%d", &mvaxis);
			if(mvaxis==0)
				run(X_DIR,X_STP);
			else if(mvaxis==1)
				run(Y_DIR,Y_STP);
			else if(mvaxis==2)
				run(Z_DIR,Z_STP);
			else if(mvaxis==3)
				run(A_DIR,A_STP);
			else if(mvaxis==4)
				run(B_DIR,B_STP);
			else if(mvaxis>4) {printf ("bad axis. Bye.\n"); exit(0);}
			break;

		case 15:
			printf("reading from port");
			read_from_port();
			sleep(1);
			break;

		case 16:
			printf("writing to  port choose data");
			printf("\nchose head 'A' or 'B'");
			scanf(" %c",&head);
			printf("\nPWM value");
			scanf("%d",&pwm);
			printf("\nmotor_en 1 or 0");
			scanf("%d",&m_en);
			printf("\nmotor_microstep 0 to 7");
			scanf("%d",&microstep);
			m1=(microstep&4)>>2;
			m2=(microstep&2)>>1;
			m3=(microstep&1)>>0;
			sprintf(str,"X,%c,%d,%d,%d,%d,%d,Z",head,pwm,m_en,m1,m2,m3);
			write_to_port(str);
			sleep(1);
			break;
		case 17:
			test_rtd();
			break;
		case 18:
			test_counters();
			break;
		case 19:
			test_gpios();
			break;
		case 20:
			printf ("Which axis? ( X=0, Y=1, Z=2, A=3, B=4 ): ");scanf ("%d", &mvaxis);
			if(mvaxis==0)
				test_run(X_DIR,X_STP);
			else if(mvaxis==1)
				test_run(Y_DIR,Y_STP);
			else if(mvaxis==2)
				test_run(Z_DIR,Z_STP);
			else if(mvaxis==3)
				test_run(A_DIR,A_STP);
			else if(mvaxis==4)
				test_run(B_DIR,B_STP);
			else if(mvaxis>4) {printf ("bad axis. Bye.\n"); exit(0);}
			break;

		case 21:
			printf("reading from port");
			test_read_from_port();
			sleep(1);
			break;

		case 22:
			printf("writing to  port choose data");
			printf("\nchose head 'A' or 'B'");
			scanf(" %c",&head);
			printf("\nPWM value");
			scanf("%d",&pwm);
			printf("\nmotor_en 1 or 0");
			scanf("%d",&m_en);
			printf("\nmotor_microstep 0 to 7");
			scanf("%d",&microstep);
			m1=(microstep&4)>>2;
			m2=(microstep&2)>>1;
			m3=(microstep&1)>>0;
			sprintf(str,"X,%c,%d,%d,%d,%d,%d,Z",head,pwm,m_en,m1,m2,m3);
			test_write_to_port(str);
			sleep(1);
			break;
		case 23:
			printf ("RESET_RTD (1) ? ");
			set_resetrtd(rst_rtd);
			break;
		case 24:
			printf ("TESTMODE (1) ? ");
			set_testmode(test);
			break;


		default:
			printf ("I can't do what you want. Bye.\n");
	}
}
