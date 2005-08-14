#ifndef __BBA_DBG_H__
#define __BBA_DBG_H__

#include "uip.h"

struct uip_netif;

typedef void* dev_s;

dev_s bba_create(struct uip_netif *dev);
s8_t bba_init(struct uip_netif *dev);
void bba_poll(struct uip_netif *dev);

#endif
