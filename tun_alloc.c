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
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdarg.h>

#include "tun_alloc.h"

int tun_alloc(char* dev, int flags)
{
	struct ifreq ifr;
	int fd, err;
	char* clonedev = "/dev/net/tun";

	if((fd = open(clonedev, O_RDWR)) < 0)
	{
		return fd;
	}

	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_flags = flags; //IF_TUN or IFF_TAP, plus maybe IFF_NO_PI

	if(*dev)
	{
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}

	if((err = ioctl(fd, TUNSETIFF, (void*) &ifr)) < 0)
	{
		close(fd);
		return err;
	}

	strcpy(dev, ifr.ifr_name);

	return fd;
}