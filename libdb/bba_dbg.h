#ifndef __BBA_DBG_H__
#define __BBA_DBG_H__

#include <gctypes.h>

#define DBG_NORMAL			0x0000
#define DBG_CONNECTED		0x0001
#define DBG_CLOSED			0x0002
#define DBG_CLOSE			0x0004

struct debugger_state {
	u32 tx_sendlen;
	u32 state;
};

void bba_init();
u32 bba_send();
u32 bba_read();

void debugger_appcall();

#define UIP_APPCALL			debugger_appcall
#define UIP_APPSTATE_SIZE	(sizeof(struct debugger_state))

#endif
