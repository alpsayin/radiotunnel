
#ifndef TUN_ALLOC_H
#define TUN_ALLOC_H

#include <net/if.h>
#include <linux/if_tun.h>

int tun_alloc(char* dev, int flags);

#endif
