/*
 * serial.c
 *
 *  Created on: Jun 11, 2012
 *      Author: alpsayin
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termio.h>
#include <time.h>
#include <sys/fcntl.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/time.h>
#include <sys/select.h>
#include <stdint.h>

#include "lock.h"
#include "devtag-allinone.h"
#include "datacollection.h"
#include "util.h"
#include "serial.h"

unsigned char preamble[PREAMBLE_LENGTH] =
{ 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };

unsigned char syncword[SYNC_LENGTH] =
{ 0xAA, 0x55, 0xAA, 0x55 };

static int serialportFd = -1;
static long predelay;
static long postdelay;
static struct termios old, tp;

int serial_getSerialPortFd()
{
	return serialportFd;
}
int serial_openSerialPort(char* device, int baud, long tx_predelay, long tx_postdelay)
{

	char dial_tty[64];
	strcpy(dial_tty, device);
	predelay = tx_predelay;
	postdelay = tx_postdelay;

	while (!get_lock(dial_tty))
	{
		if (decrementLockRetries() == 0)
			exit(-1);
		sleep(1);
	}

	if ((serialportFd = open(devtag_get(dial_tty), O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0)
	{
		perror("bad terminal device, try another");
		exit(-1);
	}

	signal(SIGIO, SIG_IGN);

	fcntl(serialportFd, F_GETFL);
	fcntl(serialportFd, F_SETFL, O_RDWR);

	if (tcgetattr(serialportFd, &tp) < 0)
	{
		perror("Couldn't get term attributes");
		exit(-1);
	}
	old = tp;

	/* 8 bits + baud rate + local control */
	tp.c_cflag = baud | CS8 | CLOCAL | CREAD;
	tp.c_cflag &= ~PARENB;
	tp.c_cflag &= ~CSTOPB;
	tp.c_cflag &= ~CSIZE;
	tp.c_cflag |= CS8;
	tp.c_cflag |= CRTSCTS;
	tp.c_oflag = 0; /* Raw Input */
	tp.c_lflag = 0; /* No conoical */
	tp.c_cc[VTIME] = 0;
	tp.c_cc[VMIN] = 1;

	/* ignore CR, ignore parity */
	//ISTRIP is a dangerous flag, it strips the 8th bit of bytes
	//tp.c_iflag= ~( ISTRIP ) | IGNPAR | IGNBRK;
	tp.c_iflag = ~(IGNBRK | PARMRK | INPCK | ISTRIP | INLCR | IUCLC | IXOFF | IXON | ICRNL | IGNCR) | BRKINT | IGNPAR | ISIG | ICANON;

	/* set output and input baud rates */

	cfsetospeed(&tp, baud);
	cfsetispeed(&tp, baud);

	tcflush(serialportFd, TCIFLUSH);
	tcflush(serialportFd, TCOFLUSH);

	if (tcsetattr(serialportFd, TCSANOW, &tp) < 0)
	{
		perror("Couldn't set term attributes");
	}

	serial_setRTS(1);

	return serialportFd;
}

void serial_closeSerialPort()
{
	if (tcsetattr(serialportFd, TCSANOW, &old) < 0)
	{
		perror("Couldn't restore term attributes");
		exit(-1);
	}
	lockfile_remove();
}

int serial_setRTS(int level)
{
	int status;

	if(level)
		registerEvent("TX","enabled");
	else
		registerEvent("RX","enabled");

	if (serialportFd < 0)
	{
		printf("serialport not opened: %s\n", __FUNCTION__);
		return -1;
	}
	if (ioctl(serialportFd, TIOCMGET, &status) == -1)
	{
//		perror("setRTS()");
		return 0;
	}

	if (level)
		status |= TIOCM_RTS;
	else
		status &= ~TIOCM_RTS;

	if (ioctl(serialportFd, TIOCMSET, &status) == -1)
	{
//		perror("setRTS()");
		return 0;
	}
	return 1;
}

int serial_transmitCapsulatedData(char * transmit_buffer, int transmit_length)
{
	int i, res = 0;
	int fd_flags = 0;
	char end_flags[2]={END_OF_FILE, 0};

	fd_flags = fcntl(serialportFd, F_GETFL);
	fcntl(serialportFd, F_SETFL, O_RDWR|O_SYNC);

//	tcflush(serialportFd, TCIFLUSH);
//	tcflush(serialportFd, TCOFLUSH);

//	if(tcsetattr(serialportFd, TCSANOW, &tp)<0)
//	{
//		perror("Couldn't set term attributes");
//		return -1;
//	}

	if (serialportFd < 0)
	{
		printf("serialport not opened: %s\n", __FUNCTION__);
		return -1;
	}

	serial_setRTS(1);
	usleep(predelay);

#if 0
	res = write(serialportFd, transmit_buffer, transmit_length);
#else
	for(i = 0; i<PREAMBLE_LENGTH; i++)
	{
		res = write(serialportFd, preamble + i, 1);
		if (res < 0)
		{
			printf("serial write failed: preamble\n");
			return -2;
		}
	}
	for(i=0; i<SYNC_LENGTH; i++)
	{
		res = write(serialportFd, syncword + i, 1);
		if (res < 0)
		{
			printf("serial write failed: syncword\n");
			return -3;
		}
	}
	for (i = 0; i < transmit_length; i++)
	{
		res = write(serialportFd, transmit_buffer + i, 1);
		if (res < 0)
		{
			printf("serial write failed: data\n");
			return -4;
		}
	}

	for(i=0; i<2; i++)
	{
		res = write(serialportFd, end_flags + i, 1);
		if (res < 0)
		{
			printf("serial write failed: end\n");
			return -5;
		}
	}
#endif
	if (res < 0)
	{
		return -6;
	}
	//wait for the buffer to be flushed
	tcdrain(serialportFd);
	usleep(postdelay);

	serial_setRTS(0);

	fcntl(serialportFd, F_GETFL);
	fcntl(serialportFd, F_SETFL, fd_flags|O_RDWR|O_ASYNC);

	//print_time("data sent");

	return 0;
}

int serial_transmitSerialData(char* transmit_buffer, int transmit_length)
{
	int i, res = 0;
	int fd_flags = 0;

	fd_flags = fcntl(serialportFd, F_GETFL);
	fcntl(serialportFd, F_SETFL, O_RDWR|O_SYNC);

//	tcflush(serialportFd, TCIFLUSH);
//	tcflush(serialportFd, TCOFLUSH);

//	if(tcsetattr(serialportFd, TCSANOW, &tp)<0)
//	{
//		perror("Couldn't set term attributes");
//		return -1;
//	}

	if (serialportFd < 0)
	{
		printf("serialport not opened: %s\n", __FUNCTION__);
		return -1;
	}

	serial_setRTS(1);
	usleep(predelay);

#if 0
	res = write(serialportFd, transmit_buffer, transmit_length);
#else
	for (i = 0; i < transmit_length; i++)
	{
		res = write(serialportFd, transmit_buffer + i, 1);
		//fprintf(stdout, "%c", io[i]);
		// Potential preamble problem??
		//usleep(10000);
		if (res < 0)
		{
			printf("serial write failed\n");
			return -3;
		}
	}
#endif
	if (res < 0)
	{
		return -2;
	}
	//wait for the buffer to be flushed
	tcdrain(serialportFd);
	usleep(postdelay);

	serial_setRTS(0);

	fcntl(serialportFd, F_GETFL);
	fcntl(serialportFd, F_SETFL, fd_flags|O_RDWR|O_ASYNC);

	//print_time("data sent");

	return 0;
}
