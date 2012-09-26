/*
 * util.c
 *
 *  Created on: Apr 29, 2012
 *      Author: alpsayin
 */


#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"

void usage(void)
{
	printf("\n");
	printf("radiotunnel radiomode[uhf|vhf] interface_name ip/subnet serial_device [ax25_destination]\n");
	printf("Example: radiotunnel uhf radio0 10.0.0.1/24 /dev/ttyUSB0\n");
	printf("\n");
}
void print_time(char* prefix)
{
	struct timeval curtime;
    gettimeofday(&curtime, NULL);
    printf("%s %ld:%ld:%ld.%ld.%ld\n", prefix, GMT_OFFSET+(curtime.tv_sec%(3600*24))/3600,(curtime.tv_sec%3600)/60, curtime.tv_sec%60, curtime.tv_usec/1000, curtime.tv_usec%1000);
}
int sprint_time(char* outbuf, char* prefix, char* postfix)
{
	struct timeval curtime;
    gettimeofday(&curtime, NULL);
    return sprintf(outbuf, "%s-%ld:%ld:%ld.%ld.%ld%s\0", prefix, GMT_OFFSET+(curtime.tv_sec%(3600*24))/3600,(curtime.tv_sec%3600)/60, curtime.tv_sec%60, curtime.tv_usec/1000, curtime.tv_usec%1000, postfix);
}
uint8_t text_to_ip(uint8_t* in_and_out, uint8_t in_length)
{
	uint8_t i;
	uint8_t point=0;
	uint8_t sum=0;
	for(i=0; i<in_length; i++)
	{
		if(in_and_out[i]=='.' || in_and_out[i]==':' || in_and_out[i]==0x00)
		{
			in_and_out[point++]=sum;
			sum=0;
//			printf("\n");
		}
		else
		{
			sum = (sum*10) + (in_and_out[i]-'0');
//			printf("sum=%d ", sum);
		}
	}
	return 0;
}
uint8_t readnline(FILE* fptr, uint8_t* out, uint8_t length)
{
	uint8_t previ=0, i=0;
	do
	{
		previ=i;
		i+=fread(out+i, 1, 1, fptr);
	} while(out[i-1]!='\n' && i<length && previ!=i);
	out[i-1]=0;
	return 0;
}
void print_callsign(uint8_t* callsign)
{
	uint8_t i;
	for(i=0; i<6; i++)
	{
		if(callsign[i]>=32 && callsign[i]<=125 )
			putchar(callsign[i]);
	}
	printf("%d", callsign[6]);
	putchar('\n');
}
void print_addr_hex(uint8_t* addr)
{
	printf("%x:%x:%x:%x:%x:%x\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}
void print_addr_dec(uint8_t* addr)
{
	printf("%d.%d.%d.%d.%d.%d\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}
