/*
 * tunclient.h
 *
 *  Created on: Jun 11, 2012
 *      Author: alpsayin
 */

#ifndef TUNCLIENT_H_
#define TUNCLIENT_H_

#define IF_NAME 			"radio0"
#define IF_SUBNET 			"10.0.1.1/24"
#define IF_MTU				"256"
#define IF_QUEUE_LENGTH		"256"

#define RADIOTUNNEL_BIM2A_DEFAULT_BAUD_RATE     				B19200
#define RADIOTUNNEL_BIM2A_TX_PREDELAY							5000ul
#define RADIOTUNNEL_BIM2A_TX_POSTDELAY							200000ul
#define RADIOTUNNEL_BIM2A_TX_MESSAGE_INTERVAL					400000ul
#define RADIOTUNNEL_BIM2A_MAX_ALLOWED_TRANSMISSION_TIME			0

#define RADIOTUNNEL_UHX1_DEFAULT_BAUD_RATE     					B2400
#define RADIOTUNNEL_UHX1_TX_PREDELAY         					1000000ul
#define RADIOTUNNEL_UHX1_TX_POSTDELAY          					500000ul
#define RADIOTUNNEL_UHX1_TX_MESSAGE_INTERVAL  					600000ul
#define RADIOTUNNEL_UHX1_MAX_ALLOWED_TRANSMISSION_TIME			2

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

#endif /* TUNCLIENT_H_ */
