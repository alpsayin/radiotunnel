/*
 * serial.h
 *
 *  Created on: Jun 11, 2012
 *      Author: alpsayin
 */

#ifndef SERIAL_H_
#define SERIAL_H_

#endif /* SERIAL_H_ */

#define PREAMBLE_LENGTH 10
#define SYNC_LENGTH 4
#define END_OF_FILE 28
#define CTRLD  4

int serial_getSerialPortFd();
int serial_openSerialPort(char* device, int baud, long tx_predelay, long tx_postdelay);
void serial_closeSerialPort();
int serial_setRTS(int level);
int serial_transmitCapsulatedData(char * transmit_buffer, int transmit_length);
int serial_transmitSerialData(char* transmit_buffer, int transmit_length);

