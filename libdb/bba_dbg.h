#ifndef __BBA_DBG_H__
#define __BBA_DBG_H__

#include <gctypes.h>

#define DBG_CONNECTED		0x0001
#define DBG_CLOSE			0x0002

struct debugger_state {
	u8 state;
	u32 alive;
};

void bba_init();
u32 bba_send();
u32 bba_read();

void debugger_appcall();

#define UIP_APPCALL			debugger_appcall
#define UIP_APPSTATE_SIZE	(sizeof(struct debugger_state))

#endif
