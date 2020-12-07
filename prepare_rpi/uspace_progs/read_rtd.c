#include <stdio.h>

#define NUM_RTDS 5
#define RTD_XFERLEN 9
char rx_rtdbuf[NUM_RTDS][RTD_XFERLEN];

void main()
{
	FILE *fp;
	int i,j;
	struct timeval st, et;

	fp = fopen ("/proc/aha_rtd", "rb");
	gettimeofday(&st,NULL);
	fread (rx_rtdbuf, sizeof(rx_rtdbuf), 1, fp);
	gettimeofday(&et,NULL);

	for (i=0; i<NUM_RTDS; i++)
	{
		for (j=0; j<RTD_XFERLEN; j++)
			printf ("%02x ", rx_rtdbuf[i][j]);
		printf ("\n");
	}
	printf("\n");
	int elapsed = ((et.tv_sec - st.tv_sec) * 1000000) + (et.tv_usec - st.tv_usec)
		printf("time: %d micro seconds\n",elapsed);
}
