#include <stdlib.h>
#include <stdio.h>
#include "asm.h"
#include "processor.h"
#include "irq.h"
#include "dsp.h"

//#define _DSP_DEBUG

// DSPCR bits
#define DSPCR_DSPRESET      0x0800        // Reset DSP
#define DSPCR_ARDMA         0x0200        // ARAM dma in progress, if set
#define DSPCR_DSPINTMSK     0x0100        // * interrupt mask   (RW)
#define DSPCR_DSPINT        0x0080        // * interrupt active (RWC)
#define DSPCR_ARINTMSK      0x0040
#define DSPCR_ARINT         0x0020
#define DSPCR_AIINTMSK      0x0010
#define DSPCR_AIINT         0x0008
#define DSPCR_HALT          0x0004        // halt DSP
#define DSPCR_PIINT         0x0002        // assert DSP PI interrupt
#define DSPCR_RES           0x0001        // reset DSP

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

static u32 __dsp_inited = FALSE;
static u32 __dsp_rudetask_pend = FALSE;
static dsptask_t *curr_task,*last_task,*first_task,*tmp_task,*rude_task;

static vu16* const _dspReg = (u16*)0xCC005000;

extern void __MaskIrq(u32);
extern void __UnmaskIrq(u32);

static void __dsp_inserttask(dsptask_t *task)
{
	dsptask_t *t;

	if(!first_task) {
		curr_task = task;
		last_task = task;
		first_task = task;
		task->next = NULL;
		task->prev = NULL;
		return;
	}

	t = first_task;
	while(t) {
		if(task->prio<t->prio) {
			task->prev = t->prev;
			t->prev = task;
			task->next = t;
			if(!task->prev) {
				first_task = task;
				break;
			} else {
				task->prev->next = task;
				break;
			}
		}
		t = t->next;
	}
	if(t) return;

	last_task->next = task;
	task->next = NULL;
	task->prev = last_task;
	last_task = task;
}

static void __dsp_removetask(dsptask_t *task)
{
	task->flags = 0;
	task->state = DSPTASK_DONE;
	if(first_task==task) {
		if(task->next) {
			first_task = task->next;
			first_task->prev = NULL;
			return;
		}
		curr_task = NULL;
		last_task = NULL;
		first_task = NULL;
		return;
	}
	if(last_task==task) {
		last_task = task->prev;
		last_task->next = NULL;
		curr_task = first_task;
		return;		
	}
	curr_task = curr_task->next;
}

static void __dsp_boottask(dsptask_t *task)
{
	u32 mail;
#ifdef _DSP_DEBUG
	printf("__dsp_boottask(%p)\n",task);
#endif
	while(!DSP_CheckMailFrom());
	mail = DSP_ReadMailFrom();
#ifdef _DSP_DEBUG
	if(mail!=0x8071FEED) {			//if the overflow calculation applies here too, this should be the value which the dsp should deliver on succesfull sync.
		printf("__dsp_boottask(): failed to sync DSP on boot (%08x)\n",mail);
	}
#endif
	DSP_SendMailTo(0x80F3A001);
	while(DSP_CheckMailTo());
	DSP_SendMailTo((u32)task->iram_maddr);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0x80F3C002);
	while(DSP_CheckMailTo());
	DSP_SendMailTo((task->iram_addr&0xffff));
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0x80F3A002);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(task->iram_len);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0x80F3B002);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0x80F3D001);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(task->init_vec);
	while(DSP_CheckMailTo());
}

static void __dsp_exectask(dsptask_t *exec,dsptask_t *hire)
{
#ifdef _DSP_DEBUG
	printf("__dsp_exectask(%p,%p)\n",exec,hire);
#endif
	if(!exec) {
		DSP_SendMailTo(0);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(0);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(0);
		while(DSP_CheckMailTo());
	} else {
		DSP_SendMailTo((u32)exec->dram_maddr);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(exec->dram_len);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(exec->dram_addr);
		while(DSP_CheckMailTo());
	}

	DSP_SendMailTo((u32)hire->iram_maddr);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(hire->iram_len);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(hire->iram_addr);
	while(DSP_CheckMailTo());
	if(hire->state==DSPTASK_INIT) {
		DSP_SendMailTo(hire->init_vec);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(0);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(0);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(0);
		while(DSP_CheckMailTo());
		return;
	}
	
	DSP_SendMailTo(hire->resume_vec);
	while(DSP_CheckMailTo());

	DSP_SendMailTo((u32)hire->dram_maddr);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(hire->dram_len);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(hire->dram_addr);
	while(DSP_CheckMailTo());
}

static void __dsp_handler(u32 nIrq,void *pCtx)
{
	u32 mail;
#ifdef _DSP_DEBUG
	printf("__dsp_handler()\n");
#endif
	_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT))|DSPCR_DSPINT;
	
	while(!DSP_CheckMailFrom());
	
	mail = DSP_ReadMailFrom();
#ifdef _DSP_DEBUG
	printf("__dsp_handler(mail = 0x%08x)\n",mail);
#endif
	if(curr_task->flags&DSPTASK_CANCEL) {
		if(mail==0xDCD10002) mail = 0xDCD10003;
	}

	switch(mail) {
		case 0xDCD10000:
			curr_task->state = DSPTASK_RUN;
			if(curr_task->init_cb) curr_task->init_cb(curr_task);
			break;
		case 0xDCD10001:
			curr_task->state = DSPTASK_RUN;
			if(curr_task->res_cb) curr_task->res_cb(curr_task);
			break;
		case 0xDCD10002:
			if(__dsp_rudetask_pend==TRUE) {
				if(rude_task==curr_task) {
					DSP_SendMailTo(0xCDD10003);
					while(DSP_CheckMailTo());
					
					rude_task = NULL;
					__dsp_rudetask_pend = FALSE;
					if(curr_task->res_cb) curr_task->res_cb(curr_task);
				} else {
					DSP_SendMailTo(0xCDD10001);
					while(DSP_CheckMailTo());
					
					__dsp_exectask(curr_task,rude_task);
					curr_task->flags = DSPTASK_YIELD;
					curr_task = rude_task;
					rude_task = NULL;
					__dsp_rudetask_pend = FALSE;
				}
			} else if(curr_task->next==NULL) {
				if(first_task==curr_task) {
					DSP_SendMailTo(0xCDD10003);
					while(DSP_CheckMailTo());

					if(curr_task->res_cb) curr_task->res_cb(curr_task);
				} else {
					DSP_SendMailTo(0xCDD10001);
					while(DSP_CheckMailTo());

					__dsp_exectask(curr_task,first_task);
					curr_task->state = DSPTASK_YIELD;
					curr_task = first_task;
				}
			} else {
				DSP_SendMailTo(0xCDD10001);
				while(DSP_CheckMailTo());
				
				__dsp_exectask(curr_task,curr_task->next);
				curr_task->state = DSPTASK_YIELD;
				curr_task = curr_task->next;
			}
			break;
		case 0xDCD10003:
			if(__dsp_rudetask_pend==TRUE) {
				if(curr_task->done_cb) curr_task->done_cb(curr_task);
				DSP_SendMailTo(0xCDD10001);
				while(DSP_CheckMailTo());
				
				__dsp_exectask(NULL,rude_task);
				__dsp_removetask(curr_task);
				
				curr_task = rude_task;
				__dsp_rudetask_pend = FALSE;
				rude_task = NULL;
			} else if(curr_task->next==NULL) {
				if(first_task==curr_task) {
					if(curr_task->done_cb) curr_task->done_cb(curr_task);
					DSP_SendMailTo(0xCDD10002);
					while(DSP_CheckMailTo());

					curr_task->state = DSPTASK_DONE;
					__dsp_removetask(curr_task);
				}
			} else {
				if(curr_task->done_cb) curr_task->done_cb(curr_task);
				
				DSP_SendMailTo(0xCDD10001);
				while(DSP_CheckMailTo());

				curr_task->state = DSPTASK_DONE;
				__dsp_exectask(NULL,first_task);
				curr_task = first_task;
				__dsp_removetask(last_task);
			}
			break;
		case 0xDCD10004:
			if(curr_task->req_cb) curr_task->req_cb(curr_task);
			break;
	}

}

void DSP_Init()
{
	u32 level;
#ifdef _DSP_DEBUG
	printf("DSP_Init()\n");
#endif
	_CPU_ISR_Disable(level);	
	if(__dsp_inited==FALSE) {
		IRQ_Request(IRQ_DSP_DSP,__dsp_handler,NULL);
		__UnmaskIrq(IRQMASK(IRQ_DSP_DSP));

		_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT))|DSPCR_DSPRESET;
		_dspReg[5] = (_dspReg[5]&~(DSPCR_HALT|DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT));
		
		curr_task = NULL;
		first_task = NULL;
		last_task = NULL;
		tmp_task = NULL;
		__dsp_inited = TRUE;
	}
	_CPU_ISR_Restore(level);	
}

u32 DSP_CheckMailTo()
{
	u32 sent_mail;
	sent_mail = _SHIFTR(_dspReg[0],15,1);
#ifdef _DSP_DEBUG
	printf("DSP_CheckMailTo(%02x)\n",sent_mail);
#endif
	return sent_mail;
}

u32 DSP_CheckMailFrom()
{
	u32 has_mail;
	has_mail = _SHIFTR(_dspReg[2],15,1);
#ifdef _DSP_DEBUG
	printf("DSP_CheckMailFrom(%02x)\n",has_mail);
#endif
	return has_mail;
}

u32 DSP_ReadMailFrom()
{
	u32 mail;
	mail = (_SHIFTL(_dspReg[2],16,16)|(_dspReg[3]&0xffff));
#ifdef _DSP_DEBUG
	printf("DSP_ReadMailFrom(%08x)\n",mail);
#endif
	return mail;
}

void DSP_SendMailTo(u32 mail)
{
#ifdef _DSP_DEBUG
	printf("DSP_SendMailTo(%08x)\n",mail);
#endif
	_dspReg[0] = _SHIFTR(mail,16,16);
	_dspReg[1] = (mail&0xffff);
}

u32 DSP_ReadCPUtoDSP()
{
	u32 cpu_dsp;
	cpu_dsp = (_SHIFTL(_dspReg[0],16,16)|(_dspReg[1]&0xffff));
#ifdef _DSP_DEBUG
	printf("DSP_ReadCPUtoDSP(%08x)\n",cpu_dsp);
#endif
	return cpu_dsp;
}

void DSP_AssertInt()
{
	u32 level;
#ifdef _DSP_DEBUG
	printf("DSP_AssertInt()\n");
#endif
	_CPU_ISR_Disable(level);
	_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT))|DSPCR_PIINT;
	_CPU_ISR_Restore(level);
}

dsptask_t* DSP_AddTask(dsptask_t *task)
{
	u32 level;
#ifdef _DSP_DEBUG
	printf("DSP_AddTask(%p)\n",task);
#endif
	_CPU_ISR_Disable(level);
	__dsp_inserttask(task);
	task->state = DSPTASK_INIT;
	task->flags = DSPTASK_ATTACH;
	_CPU_ISR_Restore(level);
	
	if(first_task==task) __dsp_boottask(task);
	return task;
}
