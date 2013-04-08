#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdarg.h>
#include <termios.h>
#include <time.h>

#include "tun_alloc.h"
#include "printAsciiHex.h"
#include "serial.h"
#include "util.h"
#include "tunclient.h"
#include "ax25.h"
#include "manchester.h"
#include "datacollection.h"

//include from serial.c
extern unsigned char syncword[SYNC_LENGTH];

#define MODIFY_LIST_LENGTH 2
const char modify[MODIFY_LIST_LENGTH][4] =
{
{ 10, 0, 0, 3 },
{ 10, 0, 1, 3 }, };

#define RESPOND_LIST_LENGTH 2
const char respond[RESPOND_LIST_LENGTH][4] =
{
{ 10, 0, 0, 2 },
{ 10, 0, 1, 2 }, };

const char nocall_callsign[7] = "NOCALL";
char my_ax25_callsign[7] = "SA0BXI\x0a";

int main(int argc, char* argv[])
{
	fd_set rfds, wfds, efds;
	struct timeval tv;
	int i;
	int tun_fd = -1, tty_fd = -1, serialPortFd = -1;
	int nread, nwrite, pid, retVal, nfds = 0;
	int sync_counter = 0;
	int sync_passed = 0;
	long baud;
	long tx_message_interval;
	long tx_predelay;
	long tx_postdelay;
	long max_allowed_tx_interval;
	unsigned char read_buffer[8192];
	unsigned char extraction_buffer[8192];
	unsigned int extraction_index = 0;
	unsigned char* manchester_buffer = NULL;
	unsigned int manchester_buffer_size;
	unsigned char* ax25_buffer = NULL;
	unsigned int ax25_buffer_size;
	unsigned int frame_length;
	char serial_device[64];
	char tun_name[IFNAMSIZ];
	char subnet[24];
	char ax25_destination[7];
	time_t last_transmission_time;
	time_t last_reception_time;

	if(argc < 2) {
		usage();
		exit(0);
	}

	if(!strcmp("uhf", argv[1]))
	{
		openLogFile(UHF_LOGFILE_NAME);
		baud = RADIOTUNNEL_BIM2A_DEFAULT_BAUD_RATE;
		tx_predelay = RADIOTUNNEL_BIM2A_TX_PREDELAY;
		tx_postdelay = RADIOTUNNEL_BIM2A_TX_POSTDELAY;
		max_allowed_tx_interval = RADIOTUNNEL_BIM2A_MAX_ALLOWED_TRANSMISSION_TIME;
		tx_message_interval = RADIOTUNNEL_BIM2A_TX_MESSAGE_INTERVAL;
	}
	else if(!strcmp("vhf", argv[1]))
	{
		openLogFile(VHF_LOGFILE_NAME);
		baud = RADIOTUNNEL_UHX1_DEFAULT_BAUD_RATE;
		tx_predelay = RADIOTUNNEL_UHX1_TX_PREDELAY;
		tx_postdelay = RADIOTUNNEL_UHX1_TX_POSTDELAY;
		max_allowed_tx_interval = RADIOTUNNEL_UHX1_MAX_ALLOWED_TRANSMISSION_TIME;
		tx_message_interval = RADIOTUNNEL_UHX1_TX_MESSAGE_INTERVAL;
	}
	else
	{
		usage();
		printf("\nwrong invocation!\n");
		exit(0);
	}


	if (argc < 3)
		strcpy(tun_name, IF_NAME);
	else
		strcpy(tun_name, argv[2]);

	if(argc < 4)
		strcpy(subnet, IF_SUBNET);
	else
		strcpy(subnet, argv[3]);

	if (argc < 5)
		strcpy(my_ax25_callsign, nocall_callsign);
	else
		strcpy(my_ax25_callsign, argv[4]);
		my_ax25_callsign[6]=atoi(argv[4]+6);

	if (argc < 6)
		strcpy(serial_device, "/dev/ttyUSB0");
	else
		strcpy(serial_device, argv[5]);

	if(argc < 7)
		ax25_get_broadcast_callsign(ax25_destination);
	else
	{
		strncpy(ax25_destination, argv[6], 6);
		ax25_destination[6]=atoi(argv[6]+6);
	}

	serialPortFd = serial_openSerialPort(serial_device, baud, tx_predelay, tx_postdelay);
	if (serialPortFd < 0)
	{
		perror("serial_openSerialPort()");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("Opened serial device \'%s\'\n", serial_device);
	}

	tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
	if (tun_fd < 0)
	{
		perror("tun_alloc()");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("Interface \'%s\' allocated\n", tun_name);
	}

	tty_fd = open("/dev/tty", O_RDONLY | O_NOCTTY);
	if (tty_fd < 0)
	{
		perror("open tty");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("/dev/tty opened\n");
	}

	pid = fork();
	if (pid == 0) //if child process
	{
		printf("Starting the interface \'%s\' with address and subnet %s\n", tun_name, subnet);
		if (execlp("ifconfig", " ", tun_name, subnet, "mtu", IF_MTU, "txqueuelen", IF_QUEUE_LENGTH "up", NULL))
			perror("interface up");
		exit(EXIT_FAILURE);
	}

	ax25_initialize_network(my_ax25_callsign);
	time(&last_transmission_time);
	time(&last_reception_time);
	serial_setRTS(0);

	while (1)
	{
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_ZERO(&efds);
		FD_SET(serialPortFd, &rfds);
		FD_SET(tun_fd, &rfds);
		FD_SET(tty_fd, &rfds);
		nfds = max(nfds, serialPortFd);
		nfds = max(nfds, tun_fd);
		nfds = max(nfds, tty_fd);
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		retVal = select(nfds + 1, &rfds, &wfds, &efds, NULL/*&tv*/);
		if (retVal < 0 && errno == EINTR)
		{
			continue;
		}
		if (retVal < 0)
		{
			perror("select()");
			exit(EXIT_FAILURE);
		}
		//TTY INTERFACE
		if (FD_ISSET(tty_fd, &rfds))
		{
			nread = read(tty_fd, read_buffer, sizeof(read_buffer));
			printf("Read %d bytes from device fd=%d\n", nread, tty_fd);
			for (i = 0; i < nread; i++)
			{
				if (read_buffer[i] == 0x1b) /*esc*/
					exit(EXIT_SUCCESS);
				putchar(read_buffer[i]);
			}
		}
		//SERIAL PORT
		if (FD_ISSET(serialPortFd, &rfds))
		{
			if ((nread = read(serialPortFd, read_buffer, sizeof(read_buffer))) > 0)
			{
				//printf("# of bytes read = %d\n", nread);
				for (i = 0; i < nread; i++)
				{
					//printf("0x%02x\n",read_buffer[i]);
					if (sync_counter < SYNC_LENGTH && read_buffer[i] == syncword[sync_counter])
					{
						sync_counter++; /* sync continued */
						//printf("sync counting %d\n", sync_counter);
					}
					else
					{
						//printf("sync reset %d -> 0x%02x 0x%02x 0x%02x 0x%02x\n", sync_counter, syncword[0], syncword[1], syncword[2], syncword[3]);
						sync_counter = 0; /* not a preamble, reset counter */
					}
					if (sync_counter >= SYNC_LENGTH && sync_passed == 0)
					{
						sync_passed = 1;
						//printf("sync_passed\n");
					}
					else if (sync_passed)
					{
						time(&last_reception_time);
						//printf("getting data '%c'\n", io[i]);
						if (read_buffer[i] == END_OF_FILE && !isManchester_encoded(read_buffer[i]))
						{
//							printf("non-manchester character received\n");
							manchester_buffer = malloc((extraction_index/2)+1);
							if(manchester_buffer == NULL)
							{
								perror("manchester_buffer malloc()");
							}
							frame_length = manchester_decode(extraction_buffer, manchester_buffer, extraction_index);
//							printf("MANCHESTER DECODED\n");
//							printAsciiHex(manchester_buffer, frame_length);
							ax25_buffer = malloc(frame_length+1);
							if(ax25_buffer == NULL)
							{
								if(manchester_buffer)
								{
									free(manchester_buffer);
									manchester_buffer=NULL;
								}
								perror("ax25_buffer malloc()");
							}
							frame_length = ax25_open_ui_packet(NULL, NULL, ax25_buffer, manchester_buffer, frame_length);
							if (frame_length)
							{
								printf("Read %d bytes from device %s\n", frame_length, serial_device);
								printAsciiHex(ax25_buffer, frame_length);
								registerEvent("RX","");
								nwrite = write(tun_fd, ax25_buffer, frame_length);
								if(nwrite < 0)
								{
									perror("error writing to tun device");
								}
							}
							else
							{
								printf("!ax.25 discarded!\n");
								registerEvent("Discard","RX");
							}
							if(ax25_buffer)
							{
								free(ax25_buffer);
								ax25_buffer = NULL;
							}
							if(manchester_buffer)
							{
								free(manchester_buffer);
								manchester_buffer = NULL;
							}
							sync_passed = 0;
							sync_counter = 0;
							extraction_index = 0;
						}
						else
						{
							//printf("saved data '%c'\n", io[i]);
//							printf("save_index=%d/%d\n",save_index, sizeof(buf));
							if (extraction_index >= sizeof(extraction_buffer))
							{
								sync_passed = 0;
								sync_counter = 0;
								extraction_index = 0;
							}
							else
							{
								extraction_buffer[extraction_index++] = read_buffer[i];
								extraction_buffer[extraction_index + 1] = 0;
							}
							//printf("-\n%s\n-\n", buf);
						}
					}
				}
			}
		}
		//TUNTAP DEVICE
		if (FD_ISSET(tun_fd, &rfds))
		{
			nread = read(tun_fd, read_buffer, sizeof(read_buffer));
			if (nread < 0)
			{
				perror("Reading from if interface");
				close(tun_fd);
				exit(EXIT_FAILURE);
			}

			printf("Read %d bytes from device %s\n", nread, tun_name);
			printAsciiHex(read_buffer, nread);

			/*
			 * ping modifier for 10.0.0.4
			 * simply swaps the last bytes of the ping packet's source and destination ips
			 * and writes it back by setting the ICMP type 0
			 */
#if PING_DEBUGGER
			for (i = 0; i < MODIFY_LIST_LENGTH; i++)
			{
				if (!memcmp(read_buffer + 16, modify[i], 4)) //ip match
				{
					if (read_buffer[9] == 1 && read_buffer[1] == 0)
					{
						if (read_buffer[20] == 8 && read_buffer[21] == 0)
						{
							read_buffer[nread] = read_buffer[15];
							read_buffer[12] = 10; //src
							read_buffer[13] = 0; //src
							read_buffer[14] = 1; //src
							read_buffer[15] = 2; //src
							read_buffer[16] = 10; //dst
							read_buffer[17] = 0; //dst
							read_buffer[18] = 1; //dst
							read_buffer[19] = 1; //dst
//							read_buffer[20] = 0;
							write(tun_fd, read_buffer, nread);
						}
					}
				}
			}
#endif
			ax25_buffer_size = AX25_TOTAL_HEADERS_LENGTH + nread + 1; //+1 for safety
			manchester_buffer_size = (ax25_buffer_size * 2) + PREAMBLE_LENGTH + SYNC_LENGTH + 2; //+2 for EOF and NULL
			ax25_buffer = malloc(ax25_buffer_size);
			if (ax25_buffer == NULL)
			{
				perror("ax25 malloc()");
			}
			manchester_buffer = malloc(manchester_buffer_size);
			if (manchester_buffer == NULL)
			{
				perror("manchester malloc()");
			}
			//encapsulate the ip packet
			frame_length = ax25_create_ui_packet(my_ax25_callsign, ax25_destination, read_buffer, nread, ax25_buffer);
			if (frame_length == 0)
			{
				printf("couldn't prepare ax25 frame");
			}
//			printf("AX25 FRAMED\n");
//			printAsciiHex(ax25_buffer, frame_length);
			//manchester encode the ax25 frame
			frame_length = manchester_encode(ax25_buffer, manchester_buffer, frame_length);
			if (frame_length == 0)
			{
				printf("couldn't manchester encode the package");
			}
			//check for last wireless activity
			if(time(NULL) <= last_transmission_time+max_allowed_tx_interval)
			{
//				sleep(1);
				registerEvent("Discard","TX");
				printf("early transmission discard\n");
			}
			else
			{
				if(time(NULL) <= last_reception_time)
				{
					usleep(tx_message_interval);
				}
				registerEvent("TX","");
				retVal=serial_transmitCapsulatedData(manchester_buffer, frame_length);
				time(&last_transmission_time);
				if(retVal)
				{
					printf("error writing to serial port device\n");
				}
			}
			if(ax25_buffer)
			{
				free(ax25_buffer);
				ax25_buffer = NULL;
			}
			if(manchester_buffer)
			{
				free(manchester_buffer);
				manchester_buffer = NULL;
			}
			/*
			 * ping responder for 10.0.0.2
			 * simply swaps the last bytes of the ping packet's source and destination ips
			 * and writes it back by setting the ICMP type 0
			 */
#if PING_DEBUGGER
			for (i = 0; i < RESPOND_LIST_LENGTH; i++)
			{
				if (!memcmp(read_buffer + 16, respond[i], 4)) //ip match
				{
					if (read_buffer[9] == 1 && read_buffer[1] == 0)
					{
						if (read_buffer[20] == 8 && read_buffer[21] == 0)
						{
							read_buffer[nread] = read_buffer[15];
							read_buffer[15] = read_buffer[19];
							read_buffer[19] = read_buffer[nread];
							read_buffer[20] = 0;
							write(tun_fd, read_buffer, nread);
						}
					}
				}
			}
#endif
		}
	}
	return 0;
}

