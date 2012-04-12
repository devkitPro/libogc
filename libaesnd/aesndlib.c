#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ogcsys.h>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>

#include "aesndlib.h"
#include "dspmixer.h"

#define PB_STRUCT_SIZE			64
#define DSP_DRAMSIZE			8192

#define VOICE_PAUSE				0x00000004
#define VOICE_LOOP				0x00000008
#define VOICE_ONCE				0x00000010
#define VOICE_STREAM			0x00000020

#define VOICE_FINISHED			0x00100000
#define VOICE_STOPPED			0x00200000
#define VOICE_RUNNING			0x40000000
#define VOICE_USED				0x80000000

struct aesndpb_t
{
	u32 out_buf;				//0

	u32 buf_start;				//4
	u32 buf_end;				//8
	u32 buf_curr;				//12

	u16 yn1;					//16
	u16 yn2;					//18
	u16 pds;					//20

	u16 freq_h;					//22
	u16 freq_l;					//24
	u16 counter;				//26

	s16 left,right;				//28,30
	u16 volume_l,volume_r;		//32,34

	u32 delay;					//36

	u32 flags;					//40

	u8 _pad[20];

	u32 mram_start;
	u32 mram_curr;
	u32 mram_end;
	u32 stream_last;

	u32 voiceno;
	u32 shift;
	AESNDVoiceCallback cb;
	
	AESNDAudioCallback audioCB;
} ATTRIBUTE_PACKED;

static dsptask_t __aesnddsptask;

static vu32 __aesndinit = 0;
static vu32 __aesndcurrab = 0;
static vu32 __aesnddspinit = 0;
static vu32 __aesndcurrvoice = 0;
static vu32 __aesnddspcomplete = 0;
static vu64 __aesnddspstarttime = 0;
static vu64 __aesnddspprocesstime = 0;
static vu32 __aesnddspabrequested = 0;
static volatile bool __aesndglobalpause = false;
static volatile bool __aesndvoicesstopped = true;

#if defined(HW_DOL)
static u32 __aesndarambase = 0;
static u32 __aesndaramblocks[MAX_VOICES];
static u32 __aesndarammemory[MAX_VOICES];
#endif

static AESNDPB __aesndvoicepb[MAX_VOICES];
static AESNDPB __aesndcommand ATTRIBUTE_ALIGN(32);
static u8 __dspdram[DSP_DRAMSIZE] ATTRIBUTE_ALIGN(32);
static u8 mute_buffer[SND_BUFFERSIZE] ATTRIBUTE_ALIGN(32);
static u8 audio_buffer[2][SND_BUFFERSIZE] ATTRIBUTE_ALIGN(32);

static __inline__ void snd_set0b(char *p,int n)
{
	while(n>0) { *p++ = 0; n--; }
}

static __inline__ void snd_set0w(int *p,int n)
{
	while(n>0) { *p++ = 0; n--; }
}

static __inline__ void __aesndcopycommand(AESNDPB *dst,AESNDPB *src)
{
	dst->buf_start = src->buf_start;
	dst->buf_end = src->buf_end;
	dst->buf_curr = src->buf_curr;

	dst->yn1 = src->yn1;
	dst->yn2 = src->yn2;
	dst->pds = src->pds;

	dst->freq_h = src->freq_h;
	dst->freq_l = src->freq_l;
	dst->counter = src->counter;

	dst->left = src->left;
	dst->right = src->right;

	dst->volume_l = src->volume_l;
	dst->volume_r = src->volume_r;

	dst->flags = src->flags;
	dst->delay = src->delay;

	dst->mram_start = src->mram_start;
	dst->mram_curr = src->mram_curr;
	dst->mram_end = src->mram_end;
	dst->stream_last = src->stream_last;

	dst->voiceno = src->voiceno;
	dst->shift = src->shift;
	dst->cb = src->cb;
}

static __inline__ void __aesndsetvoiceformat(AESNDPB *pb,u32 format)
{
	pb->flags = (pb->flags&~0x03)|(format&0x03);
	switch((format&0x03)) {
		case VOICE_MONO8:
		case VOICE_STEREO8:
			pb->shift = 0;
			break;
		case VOICE_MONO16:
		case VOICE_STEREO16:
			pb->shift = 1;
			break;
	}
}

static __inline__ void __aesndsetvoicebuffer(AESNDPB *pb,void* buffer,u32 len)
{
	pb->mram_start = (u32)buffer;
	pb->mram_curr = (u32)buffer;
	pb->mram_end = (u32)buffer + len;
}

static __inline__ void __aesndsetvoicefreq(AESNDPB *pb,u32 freq)
{
	register u32 ratio = 0x00010000*((f32)freq/(f32)DSP_DEFAULT_FREQ);
	pb->freq_h = (u16)(ratio>>16);
	pb->freq_l = (u16)(ratio&0xffff);
}

#if defined(HW_DOL)
static ARQRequest arq_request[MAX_VOICES];
static u8 stream_buffer[DSP_STREAMBUFFER_SIZE*2] ATTRIBUTE_ALIGN(32);

static void __aesndarqcallback(ARQRequest *req)
{
	__aesndvoicepb[req->owner].flags |= VOICE_RUNNING;
}

static void __aesndfillbuffer(AESNDPB *pb,u32 buffer)
{
	register u32 copy_len;
	register u32 buf_addr;

	buf_addr = __aesndaramblocks[pb->voiceno];
	if(buffer) buf_addr += DSP_STREAMBUFFER_SIZE;

	copy_len = (pb->mram_end - pb->mram_curr);
	if(copy_len>DSP_STREAMBUFFER_SIZE) copy_len = DSP_STREAMBUFFER_SIZE;

	memcpy(stream_buffer,(void*)pb->mram_curr,copy_len);
	if(copy_len<DSP_STREAMBUFFER_SIZE) memset(stream_buffer + copy_len,0,DSP_STREAMBUFFER_SIZE - copy_len);

	DCFlushRange(stream_buffer,DSP_STREAMBUFFER_SIZE);
	ARQ_PostRequestAsync(&arq_request[pb->voiceno],pb->voiceno,ARQ_MRAMTOARAM,ARQ_PRIO_HI,buf_addr,(u32)MEM_VIRTUAL_TO_PHYSICAL(stream_buffer),DSP_STREAMBUFFER_SIZE,NULL);

	pb->mram_curr += copy_len;
}

static __inline__ void __aesndhandlerequest(AESNDPB *pb)
{
	register u32 buf_addr;
	register u32 copy_len;

	if(pb->mram_curr>=pb->mram_end) {
		if(pb->flags&VOICE_STREAM && pb->cb)
			pb->cb(pb,VOICE_STATE_STREAM);
		if(pb->flags&VOICE_ONCE) {
			pb->flags |= VOICE_STOPPED;
			return;
		} else if(pb->flags&VOICE_LOOP) pb->mram_curr = pb->mram_start;
	}

	if(pb->buf_start) {
		register u32 curr_pos = pb->buf_curr;
		if(curr_pos<pb->stream_last)
			__aesndfillbuffer(pb,1);
		if(curr_pos>=(pb->buf_start + (DSP_STREAMBUFFER_SIZE>>pb->shift)) &&
		   pb->stream_last<(pb->buf_start + (DSP_STREAMBUFFER_SIZE>>pb->shift)))
			__aesndfillbuffer(pb,0);

		pb->stream_last = curr_pos;
		return;
	}

	buf_addr = __aesndaramblocks[pb->voiceno];
	pb->buf_start = buf_addr>>pb->shift;
	pb->buf_end = (buf_addr + (DSP_STREAMBUFFER_SIZE*2) - (1<<pb->shift))>>pb->shift;
	pb->buf_curr = pb->buf_start;

	copy_len = (pb->mram_end - pb->mram_curr);
	if(copy_len>(DSP_STREAMBUFFER_SIZE*2)) copy_len = (DSP_STREAMBUFFER_SIZE*2);

	memcpy(stream_buffer,(void*)pb->mram_curr,copy_len);
	if(copy_len<(DSP_STREAMBUFFER_SIZE*2)) memset(stream_buffer + copy_len,0,(DSP_STREAMBUFFER_SIZE*2) - copy_len);

	DCFlushRange(stream_buffer,(DSP_STREAMBUFFER_SIZE*2));
	ARQ_PostRequestAsync(&arq_request[pb->voiceno],pb->voiceno,ARQ_MRAMTOARAM,ARQ_PRIO_HI,buf_addr,(u32)MEM_VIRTUAL_TO_PHYSICAL(stream_buffer),(DSP_STREAMBUFFER_SIZE*2),__aesndarqcallback);

	pb->mram_curr += copy_len;
}
#elif defined(HW_RVL)
static u8 stream_buffer[MAX_VOICES][DSP_STREAMBUFFER_SIZE*2] ATTRIBUTE_ALIGN(32);

static void __aesndfillbuffer(AESNDPB *pb,u32 buffer)
{
	register u32 copy_len;
	register u32 buf_addr;

	buf_addr = (u32)stream_buffer[pb->voiceno];
	if(buffer) buf_addr += DSP_STREAMBUFFER_SIZE;

	copy_len = (pb->mram_end - pb->mram_curr);
	if(copy_len>DSP_STREAMBUFFER_SIZE) copy_len = DSP_STREAMBUFFER_SIZE;

	memcpy((void*)buf_addr,(void*)pb->mram_curr,copy_len);
	if(copy_len<DSP_STREAMBUFFER_SIZE) memset((void*)(buf_addr + copy_len),0,DSP_STREAMBUFFER_SIZE - copy_len);

	DCFlushRange((void*)buf_addr,DSP_STREAMBUFFER_SIZE);

	pb->mram_curr += copy_len;
}

static __inline__ void __aesndhandlerequest(AESNDPB *pb)
{
	register u32 buf_addr;
	register u32 copy_len;

	if(pb->mram_curr>=pb->mram_end) {
		if(pb->flags&VOICE_STREAM && pb->cb)
			pb->cb(pb,VOICE_STATE_STREAM);
		if(pb->flags&VOICE_ONCE) {
			pb->buf_start = 0;
			pb->flags |= VOICE_STOPPED;
			return;
		} else if(pb->flags&VOICE_LOOP) pb->mram_curr = pb->mram_start;
	}

	if(pb->buf_start) {
		register u32 curr_pos = pb->buf_curr;
		if(curr_pos<pb->stream_last)
			__aesndfillbuffer(pb,1);
		if(curr_pos>=(pb->buf_start + (DSP_STREAMBUFFER_SIZE>>pb->shift)) &&
		   pb->stream_last<(pb->buf_start + (DSP_STREAMBUFFER_SIZE>>pb->shift)))
			__aesndfillbuffer(pb,0);

		pb->stream_last = curr_pos;
		return;
	}

	buf_addr = (u32)MEM_VIRTUAL_TO_PHYSICAL(stream_buffer[pb->voiceno]);
	pb->buf_start = buf_addr>>pb->shift;
	pb->buf_end = (buf_addr + (DSP_STREAMBUFFER_SIZE*2) - (1<<pb->shift))>>pb->shift;
	pb->buf_curr = pb->buf_start;

	copy_len = (pb->mram_end - pb->mram_curr);
	if(copy_len>(DSP_STREAMBUFFER_SIZE*2)) copy_len = (DSP_STREAMBUFFER_SIZE*2);

	memcpy(stream_buffer[pb->voiceno],(void*)pb->mram_curr,copy_len);
	if(copy_len<(DSP_STREAMBUFFER_SIZE*2)) memset(stream_buffer[pb->voiceno] + copy_len,0,(DSP_STREAMBUFFER_SIZE*2) - copy_len);

	DCFlushRange(stream_buffer[pb->voiceno],(DSP_STREAMBUFFER_SIZE*2));

	pb->mram_curr += copy_len;
	pb->flags |= VOICE_RUNNING;
}
#endif

static void __dsp_initcallback(dsptask_t *task)
{
	DSP_SendMailTo(0xface0080);
	while(DSP_CheckMailTo());

	DSP_SendMailTo(MEM_VIRTUAL_TO_PHYSICAL(&__aesndcommand));
	while(DSP_CheckMailTo());

	__aesnddspinit = 1;
	__aesnddspcomplete = 1;
}

static void __dsp_resumecallback(dsptask_t *task)
{
}

static void __dsp_requestcallback(dsptask_t *task)
{
	if(__aesnddspabrequested==1) {
		__aesnddspprocesstime = (gettime() - __aesnddspstarttime);
		__aesnddspabrequested = 0;
		__aesnddspcomplete = 1;
		return;
	}

	DCInvalidateRange(&__aesndcommand,PB_STRUCT_SIZE);

	if(__aesndcommand.flags&VOICE_FINISHED) {
		__aesndcommand.flags &= ~VOICE_FINISHED;

		__aesndhandlerequest(&__aesndcommand);

		if(__aesndcommand.flags&VOICE_STOPPED && __aesndcommand.cb) __aesndcommand.cb(&__aesndcommand,VOICE_STATE_STOPPED);

		__aesndcopycommand(&__aesndvoicepb[__aesndcurrvoice],&__aesndcommand);

		__aesndcurrvoice++;
		while(__aesndcurrvoice<MAX_VOICES && (!(__aesndvoicepb[__aesndcurrvoice].flags&VOICE_USED) || (__aesndvoicepb[__aesndcurrvoice].flags&VOICE_STOPPED))) __aesndcurrvoice++;
		if(__aesndcurrvoice<MAX_VOICES) {
			__aesndcopycommand(&__aesndcommand,&__aesndvoicepb[__aesndcurrvoice]);
			
			if(__aesndcommand.cb) __aesndcommand.cb(&__aesndcommand,VOICE_STATE_RUNNING);

			DCFlushRange(&__aesndcommand,PB_STRUCT_SIZE);
			DSP_SendMailTo(0xface0020);
			while(DSP_CheckMailTo());
			return;
		}
	}

	if(__aesndcurrvoice>=MAX_VOICES) {
		DSP_SendMailTo(0xface0100);
		while(DSP_CheckMailTo());

		__aesnddspabrequested = 1;
	}
}

static void __dsp_donecallback(dsptask_t *task)
{
	__aesnddspinit = 0;
	__aesnddspcomplete = 0;
	__aesnddspabrequested = 0;
}

static void __audio_dma_callback()
{
	void *ptr;

	__aesndcurrab ^= 1;
	if(__aesndglobalpause==true || __aesndvoicesstopped==true)
		ptr = mute_buffer;
	else
		ptr = audio_buffer[__aesndcurrab];

	if(__aesndcommand.audioCB) __aesndcommand.audioCB(ptr,SND_BUFFERSIZE);
	AUDIO_InitDMA((u32)ptr,SND_BUFFERSIZE);

	if(__aesndglobalpause==true) return;
	if(!__aesnddspcomplete || !__aesnddspinit) return;

	__aesndcurrvoice = 0;
	__aesnddspprocesstime = 0;
	while(__aesndcurrvoice<MAX_VOICES && (!(__aesndvoicepb[__aesndcurrvoice].flags&VOICE_USED) || (__aesndvoicepb[__aesndcurrvoice].flags&VOICE_STOPPED))) __aesndcurrvoice++;
	if(__aesndcurrvoice>=MAX_VOICES) {
		__aesndvoicesstopped = true;
		return;
	}

	__aesnddspcomplete = 0;
	__aesndvoicesstopped = false;
	__aesndcopycommand(&__aesndcommand,&__aesndvoicepb[__aesndcurrvoice]);

	if(__aesndcommand.cb) __aesndcommand.cb(&__aesndcommand,VOICE_STATE_RUNNING);

	__aesndcommand.out_buf = (u32)MEM_VIRTUAL_TO_PHYSICAL(audio_buffer[__aesndcurrab]);
	DCFlushRange(&__aesndcommand,PB_STRUCT_SIZE);

	__aesnddspstarttime = gettime();
	DSP_SendMailTo(0xface0010);
	while(DSP_CheckMailTo());
}


static void __aesndloaddsptask(dsptask_t *task,const void *dsp_code,u32 dsp_code_size,const void *dram_image,u32 dram_size)
{
	task->prio = 255;
	task->iram_maddr = (void*)MEM_VIRTUAL_TO_PHYSICAL(dsp_code);
	task->iram_len = dsp_code_size;
	task->iram_addr = 0x0000;
	task->dram_maddr = (void*)MEM_VIRTUAL_TO_PHYSICAL(dram_image);
	task->dram_len = dram_size;
	task->dram_addr = 0x0000;
	task->init_vec = 0x0010;
	task->res_cb = __dsp_resumecallback;
	task->req_cb = __dsp_requestcallback;
	task->init_cb = __dsp_initcallback;
	task->done_cb = __dsp_donecallback;
	DSP_AddTask(task);
}

void AESND_Init()
{
	u32 i,level;

#if defined(HW_DOL)
	__aesndarambase = AR_Init(__aesndarammemory,MAX_VOICES);
	ARQ_Init();
#endif
	DSP_Init();
	AUDIO_Init(NULL);
	AUDIO_StopDMA();
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);

	_CPU_ISR_Disable(level);
	if(!__aesndinit) {
		__aesndinit = 1;
		__aesndcurrab = 0;
		__aesnddspinit = 0;
		__aesnddspcomplete = 0;
		__aesndglobalpause = false;
		__aesndvoicesstopped = true;

#if defined(HW_DOL)
		for(i=0;i<MAX_VOICES;i++) __aesndaramblocks[i] = AR_Alloc(DSP_STREAMBUFFER_SIZE*2);
#endif
		snd_set0w((int*)mute_buffer,SND_BUFFERSIZE>>2);
		snd_set0w((int*)audio_buffer[0],SND_BUFFERSIZE>>2);
		snd_set0w((int*)audio_buffer[1],SND_BUFFERSIZE>>2);
		DCFlushRange(mute_buffer,SND_BUFFERSIZE);
		DCFlushRange(audio_buffer[0],SND_BUFFERSIZE);
		DCFlushRange(audio_buffer[1],SND_BUFFERSIZE);

		snd_set0w((int*)&__aesndcommand,sizeof(struct aesndpb_t)>>2);
		for(i=0;i<MAX_VOICES;i++)
			snd_set0w((int*)&__aesndvoicepb[i],sizeof(struct aesndpb_t)>>2);

		__aesndloaddsptask(&__aesnddsptask,dspmixer,dspmixer_size,__dspdram,DSP_DRAMSIZE);
	}

	AUDIO_RegisterDMACallback(__audio_dma_callback);
	AUDIO_InitDMA((u32)audio_buffer[__aesndcurrab],SND_BUFFERSIZE);
	AUDIO_StartDMA();

	_CPU_ISR_Restore(level);
}

void AESND_Reset()
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(__aesndinit) {
		AUDIO_StopDMA();
		AUDIO_RegisterDMACallback(NULL);

		DSP_SendMailTo(0xfacedead);
		while(DSP_CheckMailTo());
		
		do {
			_CPU_ISR_Flash(level);
		} while(__aesnddspinit);

		__aesndinit = 0;
	}
	_CPU_ISR_Restore(level);
}

void AESND_Pause(bool pause)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__aesndglobalpause = pause;
	_CPU_ISR_Restore(level);
}

u32 AESND_GetDSPProcessTime()
{
	u32 level;
	u32 time = 0;

	_CPU_ISR_Disable(level);
	time = ticks_to_microsecs(__aesnddspprocesstime);
	_CPU_ISR_Restore(level);
	
	return time;
}

f32 AESND_GetDSPProcessUsage()
{
	u32 level;
	f32 usage = 0.0f;

	_CPU_ISR_Disable(level);
	usage = (ticks_to_microsecs(__aesnddspprocesstime)*100)/2000.0f;
	_CPU_ISR_Restore(level);
	
	return usage;
}

AESNDAudioCallback AESND_RegisterAudioCallback(AESNDAudioCallback cb)
{
	u32 level;
	AESNDAudioCallback aCB;

	_CPU_ISR_Disable(level);
	aCB = __aesndcommand.audioCB;
	__aesndcommand.audioCB = cb;
	_CPU_ISR_Restore(level);

	return aCB;
}

AESNDPB* AESND_AllocateVoice(AESNDVoiceCallback cb)
{
	u32 i,level;
	AESNDPB *pb = NULL;

	_CPU_ISR_Disable(level);
	for(i=0;i<MAX_VOICES;i++) {
		pb = &__aesndvoicepb[i];
		if(!(pb->flags&VOICE_USED)) {
			pb->voiceno = i;
			pb->flags = (VOICE_USED|VOICE_STOPPED);
			pb->pds = pb->yn1 = pb->yn2 = 0;
			pb->buf_start = 0;
			pb->buf_curr = 0;
			pb->buf_end = 0;
			pb->counter = 0;
			pb->volume_l = 255;
			pb->volume_r = 255;
			pb->freq_h = 0x0001;
			pb->freq_l = 0x0000;
			pb->cb = cb;
			break;
		}
	}
	_CPU_ISR_Restore(level);

	return pb;
}

void AESND_FreeVoice(AESNDPB *pb)
{
	u32 level;
	if(pb==NULL) return;

	_CPU_ISR_Disable(level);
	snd_set0w((int*)pb,sizeof(struct aesndpb_t)>>2);
	_CPU_ISR_Restore(level);
}

void AESND_PlayVoice(AESNDPB *pb,u32 format,const void *buffer,u32 len,u32 freq,u32 delay,bool looped)
{
	u32 level;
	void *ptr = (void*)buffer;

	_CPU_ISR_Disable(level);
	__aesndsetvoiceformat(pb,format);
	__aesndsetvoicefreq(pb,freq);
	__aesndsetvoicebuffer(pb,ptr,len);

	pb->flags &= ~(VOICE_RUNNING|VOICE_STOPPED|VOICE_LOOP|VOICE_ONCE);
	if(looped==true) 
		pb->flags |= VOICE_LOOP;
	else
		pb->flags |= VOICE_ONCE;

	pb->buf_start = pb->buf_curr = pb->buf_end = pb->stream_last = 0;
	pb->delay = (delay*48);
	pb->pds = pb->yn1 = pb->yn2 = 0;
	pb->counter = 0;
	_CPU_ISR_Restore(level);
}

void AESND_SetVoiceBuffer(AESNDPB *pb,const void *buffer,u32 len)
{
	u32 level;
	void *ptr = (void*)buffer;

	_CPU_ISR_Disable(level);
	__aesndsetvoicebuffer(pb,ptr,len);
	_CPU_ISR_Restore(level);
}

void AESND_SetVoiceFormat(AESNDPB *pb,u32 format)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__aesndsetvoiceformat(pb,format);
	_CPU_ISR_Restore(level);
}

void AESND_SetVoiceVolume(AESNDPB *pb,u16 volume_l,u16 volume_r)
{
	u32 level;

	_CPU_ISR_Disable(level);
	pb->volume_l = volume_l;
	pb->volume_r = volume_r;
	_CPU_ISR_Restore(level);
}

void AESND_SetVoiceFrequency(AESNDPB *pb,u32 freq)
{
	u32 level;
	
	_CPU_ISR_Disable(level);
	__aesndsetvoicefreq(pb,freq);
	_CPU_ISR_Restore(level);
}

void AESND_SetVoiceStream(AESNDPB *pb,bool stream)
{
	u32 level;
	
	_CPU_ISR_Disable(level);
	if(stream==true)
		pb->flags |= VOICE_STREAM;
	else
		pb->flags &= ~VOICE_STREAM;
	_CPU_ISR_Restore(level);
}

void AESND_SetVoiceLoop(AESNDPB *pb,bool loop)
{
	u32 level;
	
	_CPU_ISR_Disable(level);
	if(loop==true)
		pb->flags |= VOICE_LOOP;
	else
		pb->flags &= ~VOICE_LOOP;
	_CPU_ISR_Restore(level);
}

void AESND_SetVoiceMute(AESNDPB *pb,bool mute)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(mute==true)
		pb->flags |= VOICE_PAUSE;
	else
		pb->flags &= ~VOICE_PAUSE;
	_CPU_ISR_Restore(level);
}

void AESND_SetVoiceStop(AESNDPB *pb,bool stop)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(stop==true)
		pb->flags |= VOICE_STOPPED;
	else
		pb->flags &= ~VOICE_STOPPED;
	_CPU_ISR_Restore(level);
}

AESNDVoiceCallback AESND_RegisterVoiceCallback(AESNDPB *pb,AESNDVoiceCallback cb)
{
	u32 level;
	AESNDVoiceCallback rcb = NULL;

	_CPU_ISR_Disable(level);
	rcb = pb->cb;
	pb->cb = cb;
	_CPU_ISR_Restore(level);

	return rcb;
}
