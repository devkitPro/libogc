#ifndef __DSP_H__
#define __DSP_H__

#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef void (*DSPCallback)(void *task);

typedef struct _dsp_task {
	vu32 state;
	vu32 prio;
	vu32 flags;
	
	u16 *iram_maddr;
	u32 iram_len;
	u16 iram_addr;

	u16 *dram_maddr;
	u32 dram_len;
	u16 dram_addr;
	
	DSPCallback init_cb;
	DSPCallback res_cb;
	DSPCallback done_cb;
	DSPCallback req_cb;

	struct _dsp_task *next;
	struct _dsp_task *prev;
} dsptask_t;

void DSP_Init();
u32 DSP_CheckMailTo();
u32 DSP_CheckMailFrom();
u32 DSP_ReadMailFrom();
void DSP_AssertInt();

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
