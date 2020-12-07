#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>
#include <syslog.h>
#include <data_getter.h>

#define PORT 5000
#define MAXLINE 1000
#define ALLOWED_SLOW_JITTER 5000
#define ALLOWED_FAST_JITTER 10000

//void send_frm_client(udp_sendto_pc_t* ,int);
int create_socket(struct sockaddr_in);
void openport();
int sockfd;
int ser_fd;
struct sockaddr_in servaddr;
volatile sig_atomic_t stop;

struct stats_t
{
	int fast_data_time;
	int fast_data_maxdelay;
	int fast_data_mindelay;
	int fast_data_jitter;

	int slow_data_time;
	int slow_data_maxdelay;
	int slow_data_mindelay;
	int slow_data_jitter;

	int serial_data_time;
	int serial_data_maxdelay;
	int serial_data_mindelay;
	int serial_data_jitter;
} stats;

void inthand(int signum) {
	printf("IN-HANDLER %d\n",signum);
	stop=1;
}

void seestats(int signum) {
	printf("showing stats\n",signum);
	printf("fast thread time=%d\n",stats.fast_data_time);
	printf("fast thread maxdelay=%d\n",stats.fast_data_maxdelay);
	printf("fast thread mindelay=%d\n",stats.fast_data_mindelay);
	printf("fast thread jitter=%d\n",stats.fast_data_jitter);

	printf("slow thread time=%d\n",stats.slow_data_time);
	printf("slow thread maxdelay=%d\n",stats.slow_data_maxdelay);
	printf("slow thread mindelay=%d\n",stats.slow_data_mindelay);
	printf("slow thread jitter=%d\n",stats.slow_data_jitter);

	printf("serial thread time=%d\n",stats.serial_data_time);
	printf("serial thread maxdelay=%d\n",stats.serial_data_maxdelay);
	printf("serial thread mindelay=%d\n",stats.serial_data_mindelay);
	printf("serial thread jitter=%d\n",stats.serial_data_jitter);

}


void resetstats(int signum) {
	printf("Resetting stats\n",signum);
	memset(&stats,0,sizeof(stats));
}

void *recvthread(void *vargp)
{
	FILE *reset_fp;
	FILE *en_fp;
	FILE *axis_fp;
	FILE *mstep_fp;
	FILE *heater_fp;
	FILE *rgb_fp;
	FILE *hooter_fp;
	FILE *head_ac220v_fp;
	FILE *door_fp;
	udp_recivefrm_pc_t rx;
	int ret;
	//memset(&rx,0,sizeof(rx));
	ssize_t numbytes;

	enable_t enbuf=1;


	while(!stop)
	{
		//		printf("IN recvthread\n");
		numbytes = recvfrom(sockfd, &rx, sizeof(rx), 0, (struct sockaddr*)NULL, NULL);
		if (numbytes == sizeof (rx))
		{
			if(rx.msgtype==SENSEOS_WRITE)
			{
				mstep_fp = fopen (PROC PROC_MICROSTEP_FILENAME, "wb");
				if(mstep_fp!=NULL)
					fwrite (&rx.u.write_data.set_microstep, sizeof(rx.u.write_data.set_microstep), 1, mstep_fp);
				fclose(mstep_fp);

				axis_fp = fopen (PROC PROC_AXIS_FILENAME, "wb");
				if(axis_fp!=NULL)
					fwrite (&rx.u.write_data.en_axis, sizeof(rx.u.write_data.en_axis),1,  axis_fp);
				fclose(axis_fp);

				heater_fp = fopen (PROC PROC_HEATER_FILENAME, "wb");
				if(heater_fp!=NULL)
					fwrite (&rx.u.write_data.set_heater, sizeof(rx.u.write_data.set_heater),1,  heater_fp);
				fclose(heater_fp);

				hooter_fp = fopen (PROC PROC_HOOTER_FILENAME, "wb");
				if(hooter_fp!=NULL)
					fwrite (&rx.u.write_data.set_hooter, sizeof(rx.u.write_data.set_hooter),1,  hooter_fp);
				fclose(hooter_fp);

				head_ac220v_fp = fopen (PROC PROC_HEAD_AC220_FILENAME, "wb");
				if(head_ac220v_fp!=NULL)
					fwrite (&rx.u.write_data.set_head, sizeof(rx.u.write_data.set_head),1,  head_ac220v_fp);
				fclose(head_ac220v_fp);

				rgb_fp = fopen (PROC PROC_RGB_FILENAME, "wb");
				if(rgb_fp!=NULL)
					fwrite (&rx.u.write_data.set_rgb, sizeof(rx.u.write_data.set_rgb),1,  rgb_fp);
				fclose(rgb_fp);

				reset_fp = fopen (PROC_RESETHW_FILENAME, "wb");
				if(reset_fp!=NULL)
					fwrite (&rx.u.write_data.rst, sizeof(rx.u.write_data.rst),1,  reset_fp);
				fclose(reset_fp);

				en_fp = fopen (PROC PROC_ENABLEHW_FILENAME, "wb");
				if(en_fp!=NULL)
					fwrite (&rx.u.write_data.set_en, sizeof(rx.u.write_data.set_en),1,  en_fp);
				fclose(en_fp);
				//	printf ("en:    %d\n", rx.u.write_data.set_en);

				door_fp = fopen (PROC PROC_DOOR_FILENAME, "wb");
				if(door_fp!=NULL)
					fwrite (&rx.u.write_data.set_door, sizeof(rx.u.write_data.set_door),1,  door_fp);
				fclose(door_fp);
				//		printf ("door:    %d\n", rx.write_data.set_door);
			}
		}
	}

	//fclose(en_fp);
	close(sockfd); 
	printf("exit recvthread\n");
}

void *send_slow_thread(void *vargp)
{
	FILE *rtd_fp;
	FILE *counter_fp;
	FILE *iostatus_fp;
	FILE *stats_fp;
	struct timeval curtime,oldtime;
	gettimeofday(&curtime,NULL);
	rtd_fp = fopen (PROC PROC_RTD_FILENAME, "rb");
	iostatus_fp=fopen (PROC PROC_IOSTATUS_FILENAME, "rb");
	counter_fp = fopen (PROC PROC_COUNTERS_FILENAME, "rb");		
	stats_fp=fopen (PROC PROC_STATISTICS_FILENAME, "rb");
	udp_sendto_pc_t tx;
	tx.msgtype = SENSEOS_FS_SLOW;
	while(!stop)
	{
		//printf("IN sendthread\n");
		//READ(TX)
		oldtime = curtime;
		gettimeofday(&curtime,NULL);
		int ret,i,j;
		ret=fread (&tx.u.slow_data.rx_rtd, sizeof(tx.u.slow_data.rx_rtd), 1, rtd_fp);
		/*for (i=0; i<NUM_RTDS; i++)
		  {
		  for (j=0; j<RTD_XFERLEN; j++)
		  printf ("%02x ", tx.rx_rtd.buf[i][j]);
		  printf ("\n");
		  }
		  printf ("\n");
		  */		
		ret=fread (&tx.u.slow_data.iostatus, 1, sizeof(tx.u.slow_data.iostatus), iostatus_fp);
		//		printf ("estop:    %d\n", tx.iostatus.estop);
		//		printf ("is_enabled:    %d\n", tx.iostatus.is_enabled);
		ret=fread (&tx.u.slow_data.stats, sizeof(tx.u.slow_data.stats), 1, stats_fp);
		//printf ("i2c_xfers:   %20ld\n", tx.stats.i2c_transfers);
		sendto(sockfd, &tx, sizeof(udp_sendto_pc_t), 0, (struct sockaddr*)NULL, sizeof(servaddr)); 
		usleep(1000);
		stats.slow_data_time = curtime.tv_usec - oldtime.tv_usec;
		if(stats.slow_data_time > stats.slow_data_maxdelay)
			stats.slow_data_maxdelay = stats.slow_data_time; 
		else if(stats.slow_data_mindelay > stats.slow_data_time)
			stats.slow_data_mindelay = stats.slow_data_time;
		else	
		{
			stats.slow_data_mindelay = stats.slow_data_time;
			stats.slow_data_maxdelay = stats.slow_data_time; 
		}
		stats.slow_data_jitter = stats.slow_data_maxdelay - stats.slow_data_mindelay;
		if (stats.slow_data_jitter > ALLOWED_SLOW_JITTER)
		{
			syslog(LOG_INFO,"max jitter in slow thread %d more than %d\n",stats.slow_data_jitter,ALLOWED_SLOW_JITTER);
			resetstats(SIGUSR1);
		}

		//WRITE(RX)
		//rx=recive();
	}
	fclose(rtd_fp);
	fclose(iostatus_fp);
	fclose(stats_fp); 
	close(sockfd); 
	printf("exit send_slow_thread\n");
}

void *send_fast_thread(void *vargp)
{
	FILE *counter_fp;
	counter_fp = fopen (PROC PROC_COUNTERS_FILENAME, "rb");		
	udp_sendto_pc_t tx;
	tx.msgtype = SENSEOS_FS_FAST;
	struct timeval curtime,oldtime;
	gettimeofday(&curtime,NULL);
	while(!stop)
	{
		//printf("IN sendthread\n");
		oldtime = curtime;
		gettimeofday(&curtime,NULL);
		int ret,i,j;
		ret=fread (&tx.u.fast_data.readspi, sizeof(tx.u.fast_data.readspi), 1, counter_fp);
		/*for (i=0; i<NUM_COUNTERS; i++)
		  printf ("c%d: %08x ", i, tx.readspi.counter[i]);
		  printf ("\n");*/
		sendto(sockfd, &tx, sizeof(udp_sendto_pc_t), 0, (struct sockaddr*)NULL, sizeof(servaddr)); 
		usleep(300);
		stats.fast_data_time = curtime.tv_usec - oldtime.tv_usec;
		if(stats.fast_data_time > stats.fast_data_maxdelay)
			stats.fast_data_maxdelay = stats.fast_data_time; 
		else if(stats.fast_data_mindelay > stats.fast_data_time)
			stats.fast_data_mindelay = stats.fast_data_time;
		else	
		{
			stats.fast_data_mindelay = stats.fast_data_time;
			stats.fast_data_maxdelay = stats.fast_data_time; 
		}
		stats.fast_data_jitter = stats.fast_data_maxdelay - stats.fast_data_mindelay;
		if (stats.fast_data_jitter > ALLOWED_FAST_JITTER)
			syslog(LOG_INFO,"max jitter in fast thread %d more than %d\n",stats.fast_data_jitter,ALLOWED_FAST_JITTER);
	}
	fclose(counter_fp);
	close(sockfd); 
	printf("exit send_fast_thread\n");
}


void* serial_read(void* vargp)
{
	//READ
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(ser_fd, &fds);
	//struct timeval timeout = { 10, 0 };
	udp_sendto_pc_t tx;
	tx.msgtype = SERIAL_DATA;
	struct timeval curtime,oldtime;
	gettimeofday(&curtime,NULL);
	char rd_c[2];
	int ret;
	while(!stop)
	{
		oldtime = curtime;
		gettimeofday(&curtime,NULL);
		//printf("IN serial_read\n");
		ret=read (ser_fd, rd_c, 1);
		if(ret)
		{
			rd_c[1]='\0';
			strcat (tx.u.serial_data, rd_c);
			if(strcmp(rd_c,"Z")==0)
			{
				//send (mystr);
				sendto(sockfd, &tx, sizeof(udp_sendto_pc_t), 0, (struct sockaddr*)NULL, sizeof(servaddr)); 
				//printf("str=%s\n",tx.u.serial_data);
				tx.u.serial_data[0]='\0';
			}
		}
		stats.serial_data_time = curtime.tv_usec - oldtime.tv_usec;
		if(stats.serial_data_time > stats.serial_data_maxdelay)
			stats.serial_data_maxdelay = stats.serial_data_time; 
		else if(stats.serial_data_mindelay > stats.serial_data_time)
			stats.serial_data_mindelay = stats.serial_data_time;
		else	
		{
			stats.serial_data_mindelay = stats.serial_data_time;
			stats.serial_data_maxdelay = stats.serial_data_time; 
		}
		stats.serial_data_jitter = stats.serial_data_maxdelay - stats.serial_data_mindelay;
		if (stats.serial_data_jitter > ALLOWED_SLOW_JITTER)
			syslog(LOG_INFO,"max jitter in serial thread %d more than %d\n",stats.serial_data_jitter,ALLOWED_SLOW_JITTER);

	}
	printf("exit serial_read\n");
}

void* serial_write(void* varg)
{
	udp_recivefrm_pc_t rx;	
	memset(&rx,0,sizeof(rx));
	ssize_t numbytes;
	while(!stop)
	{
//		printf("IN serial_write\n");
		numbytes = recvfrom(sockfd, &rx, sizeof(rx), 0, (struct sockaddr*)NULL, NULL);
		if (numbytes == sizeof (rx))
		{
			if(rx.msgtype==SERIAL_DATA)
	//		puts(rx.serial_data);
			{
			write (ser_fd, rx.u.serial_data, strlen(rx.u.serial_data));
			usleep(10000);
			}
		}
		memset(&rx,0,sizeof(rx));
	}
	printf("exit serial_write\n");
}


int create_socket(struct sockaddr_in servaddr)
{
	char buffer[100];
	int sockfd, n;
	static int t=0;
	// clear servaddr
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_addr.s_addr = inet_addr("192.168.0.111");
	servaddr.sin_port = htons(PORT);
	servaddr.sin_family = AF_INET;

	// create datagram socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	// connect to server
	if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
	{
		printf("\n Error : Connect Failed \n");
		exit(0);
	}
	return sockfd;
}

void openport()
{
#define BAUDRATE B115200
#define MODEMDEVICE "/dev/ttyAMA0"/*UART NAME IN PROCESSOR*/
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1
	struct termios oldtp, newtp;
	ser_fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY |O_NDELAY );
	if (ser_fd <0)
	{
		perror(MODEMDEVICE);
	}

	fcntl(ser_fd,F_SETFL,0);
	tcgetattr(ser_fd,&oldtp); /* save current serial port settings */
	//     tcgetattr(fd,&newtp); /* save current serial port settings */
	bzero(&newtp, sizeof(newtp));
	//       bzero(&oldtp, sizeof(oldtp));

	newtp.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;

	newtp.c_iflag = IGNPAR | ICRNL;

	newtp.c_oflag = 0;
	//newtp.c_lflag = ICANON;
	newtp.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	newtp.c_cc[VINTR]    = 0;     /* Ctrl-c */
	newtp.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
	newtp.c_cc[VERASE]   = 0;     /* del */
	newtp.c_cc[VKILL]    = 0;     /* @ */
	// newtp.c_cc[VEOF]     = 4;     /* Ctrl-d */
	newtp.c_cc[VTIME]    = 0;     /* inter-character timer unused */
	newtp.c_cc[VMIN]     = 0;     /* blocking read until 1 character arrives */
	newtp.c_cc[VSWTC]    = 0;     /* '\0' */
	newtp.c_cc[VSTART]   = 0;     /* Ctrl-q */
	newtp.c_cc[VSTOP]    = 0;     /* Ctrl-s */
	newtp.c_cc[VSUSP]    = 0;     /* Ctrl-z */
	newtp.c_cc[VEOL]     = 0;     /* '\0' */
	newtp.c_cc[VREPRINT] = 0;     /* Ctrl-r */
	newtp.c_cc[VDISCARD] = 0;     /* Ctrl-u */
	newtp.c_cc[VWERASE]  = 0;     /* Ctrl-w */
	newtp.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
	newtp.c_cc[VEOL2]    = 0;     /* '\0' */

	tcflush(ser_fd, TCIFLUSH);
	tcsetattr(ser_fd,TCSANOW,&newtp);

}

int main()
{

	sockfd=create_socket(servaddr);	
	if(sockfd==-1)
	{
		printf("client socket creation failed");
		exit(0);
	}	
	openport();
	//THREADS
	signal(SIGINT,inthand);
	signal(SIGUSR1,resetstats);
	signal(SIGUSR2,seestats);
	pthread_t send_slow,send_fast,recv,serialR,serialW;
	pthread_create (&send_slow,NULL,send_slow_thread,NULL);
	pthread_create (&send_fast,NULL,send_fast_thread,NULL);
	pthread_create (&recv,NULL,recvthread,NULL);
	pthread_create (&serialR,NULL,serial_read,NULL);
	pthread_create (&serialW,NULL,serial_write,NULL);
	pthread_join(send_slow,NULL);
	pthread_join(send_fast,NULL);
	pthread_join(recv,NULL);
	pthread_join(serialR,NULL);
	pthread_join(serialW,NULL);
	exit(0);

}

