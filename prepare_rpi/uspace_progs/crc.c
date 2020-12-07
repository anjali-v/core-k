#include <string.h>
#include <stdint.h>
#include <stdio.h>

uint8_t calc_crc8(char *data)
{
		uint8_t crc=0xff;
		int i,j;
		int len=strlen(data);
		for(i=0;i<len-1;i++)
		{
				crc^=data[i];
				for(j=0;j<8;j++)
				{
						if((crc&0x80) !=0)
								crc=(uint8_t)((crc<<1)^0x31);
						else
								crc <<=1;
				}
		}
		//  return (uint8_t)data[3];
		return crc;
}
