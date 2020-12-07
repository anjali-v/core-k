// server program for udp connection 
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
#include<netinet/in.h> 
#include <data_getter.h>
#define PORT 5000 
#define MAXLINE 1000 

// Driver code 
int main() 
{
	udp_sendto_pc_t rx; 
	udp_recivefrm_pc_t tx;
	//	char *message = "Hello Client";
	//char str[100]; 
	char str[100]="X,A,100,0,0,0,0,Z"; 
	int listenfd, len,i,j; 
	static int t=0;
	struct sockaddr_in servaddr, cliaddr; 
	bzero(&servaddr, sizeof(servaddr)); 

	// Create a UDP Socket 
	listenfd = socket(AF_INET, SOCK_DGRAM, 0);		 
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
	servaddr.sin_port = htons(PORT); 
	servaddr.sin_family = AF_INET; 

	// bind server address to socket descriptor 
	bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)); 

	//receive the datagram 
	len = sizeof(cliaddr);
	memset(&rx, 0, sizeof(rx));
//	tx.set_en=1;
	while(1)
	{
		int n = recvfrom(listenfd, &rx, sizeof(udp_sendto_pc_t),0, (struct sockaddr*)&cliaddr,&len); //receive message from server 
		if(rx.msgtype==SERIAL_DATA)
		{
			printf ("string:   %s\n", rx.u.serial_data);
		}


		if(rx.msgtype==SENSEOS_FS_SLOW)
		{
			printf ("estop:    %d\n", rx.u.slow_data.iostatus.estop);
		/*	for (i=0; i<NUM_RTDS; i++)
			{
			for (j=0; j<RTD_XFERLEN; j++)
			printf ("%02x ", rx.rx_rtd.buf[i][j]);
			printf ("\n");
			}
			printf ("\n");
			*/
			//	printf ("\n");
			//	printf ("i2c_xfers:   %20d\n", rx.stats.i2c_transfers);
		}
		if(rx.msgtype==SENSEOS_FS_SLOW)
		{
			//	for (i=0; i<NUM_COUNTERS; i++)
			//               printf ("c%d: %08x ", i, rx.readspi.counter[i]);
		}
		//strcpy(tx.serial_data,str);
		tx.u.write_data.set_en=1;
		tx.msgtype=SENSEOS_WRITE;
		sendto(listenfd, &tx, sizeof(udp_recivefrm_pc_t), 0,(struct sockaddr*)&cliaddr, sizeof(cliaddr));
		//usleep(10000);
		memset(&tx, 0, sizeof(tx));
	}
} 

